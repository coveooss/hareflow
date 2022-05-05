#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "hareflow/detail/internal_types.h"

namespace hareflow::detail {

class BackgroundScheduler
{
public:
    HAREFLOW_EXPORT BackgroundScheduler(std::uint32_t max_nb_threads = 0, std::chrono::milliseconds max_idle_time = std::chrono::seconds(5));
    HAREFLOW_EXPORT ~BackgroundScheduler();

    std::future<void> run_task(std::function<void()> task);

private:
    struct WorkThread {
        bool                                      m_running = false;
        std::thread                               m_thread;
        std::optional<std::packaged_task<void()>> m_current_task;
        std::condition_variable                   m_task_assigned;
    };

    std::tuple<std::optional<std::uint32_t>, bool> find_available_thread();
    void                                           start_thread(std::uint32_t thread_num);
    std::future<void>                              assign_task(std::uint32_t thread_num, std::function<void()> task);
    void                                           run_work_thread(std::uint32_t thread_num);

    const std::uint32_t                    m_max_nb_threads;
    const std::chrono::milliseconds        m_max_idle_time;
    std::mutex                             m_mutex;
    std::vector<WorkThread>                m_work_threads;
    std::deque<std::packaged_task<void()>> m_task_overflow;
    bool                                   m_quitting;
};

}  // namespace hareflow::detail