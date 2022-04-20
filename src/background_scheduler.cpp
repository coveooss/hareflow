#include "hareflow/detail/background_scheduler.h"

namespace hareflow::detail {

BackgroundScheduler::BackgroundScheduler(std::uint32_t max_nb_threads, std::chrono::milliseconds max_idle_time)
 : m_max_nb_threads(max_nb_threads == 0 ? std::max(std::thread::hardware_concurrency(), 1u) : max_nb_threads),
   m_max_idle_time(max_idle_time),
   m_mutex(),
   m_work_threads(m_max_nb_threads),
   m_task_overflow(),
   m_quitting(false)
{
}

BackgroundScheduler::~BackgroundScheduler()
{
    {
        std::unique_lock lock(m_mutex);
        m_quitting = true;
    }
    for (auto& work_thread : m_work_threads) {
        work_thread.m_task_assigned.notify_one();
        if (work_thread.m_thread.joinable()) {
            work_thread.m_thread.join();
        }
    }
}

std::future<void> BackgroundScheduler::run_task(std::function<void()> task)
{
    std::future<void> future;

    std::unique_lock lock(m_mutex);
    auto [available_thread, started] = find_available_thread();
    if (available_thread) {
        if (!started) {
            start_thread(*available_thread);
        }
        future = assign_task(*available_thread, std::move(task));
        lock.unlock();
        m_work_threads[*available_thread].m_task_assigned.notify_one();
    } else {
        m_task_overflow.emplace_back(std::move(task));
        future = m_task_overflow.back().get_future();
    }
    return future;
}

std::tuple<std::optional<std::uint32_t>, bool> BackgroundScheduler::find_available_thread()
{
    std::optional<std::uint32_t> idle_thread;
    std::optional<std::uint32_t> first_stopped_thread;
    for (std::uint32_t i = 0; i < m_work_threads.size(); ++i) {
        auto& work_thread = m_work_threads[i];
        if (work_thread.m_running && !work_thread.m_current_task) {
            idle_thread = i;
            break;
        } else if (!work_thread.m_running && !first_stopped_thread) {
            first_stopped_thread = i;
        }
    }

    if (idle_thread) {
        return std::make_tuple(idle_thread, true);
    } else if (first_stopped_thread) {
        return std::make_tuple(first_stopped_thread, false);
    } else {
        return std::make_tuple(std::nullopt, false);
    }
}

void BackgroundScheduler::start_thread(std::uint32_t thread_num)
{
    auto& work_thread = m_work_threads[thread_num];
    if (work_thread.m_thread.joinable()) {
        work_thread.m_thread.join();
    }
    work_thread.m_running = true;
    work_thread.m_thread  = std::thread([this, thread_num]() { run_work_thread(thread_num); });
}

std::future<void> BackgroundScheduler::assign_task(std::uint32_t thread_num, std::function<void()> task)
{
    auto& work_thread          = m_work_threads[thread_num];
    work_thread.m_current_task = std::packaged_task<void()>(std::move(task));
    return work_thread.m_current_task->get_future();
}

void BackgroundScheduler::run_work_thread(std::uint32_t thread_num)
{
    std::unique_lock lock(m_mutex);
    bool             stop_thread = m_quitting;
    auto&            self        = m_work_threads[thread_num];
    while (!stop_thread) {
        if (self.m_current_task) {
            lock.unlock();
            (*self.m_current_task)();
            lock.lock();
            if (!m_task_overflow.empty()) {
                self.m_current_task = std::move(m_task_overflow.front());
                m_task_overflow.pop_front();
            } else {
                self.m_current_task = std::nullopt;
            }
        }
        bool timeout = !self.m_task_assigned.wait_for(lock, m_max_idle_time, [this, &self]() { return m_quitting || self.m_current_task; });
        stop_thread  = timeout || m_quitting;
    }
    self.m_running = false;
}

}  // namespace hareflow::detail