#include "hareflow/detail/reschedulable_task.h"

#include <future>

#include "hareflow/io_context_holder.h"
#include "hareflow/detail/background_scheduler.h"

namespace hareflow::detail {

std::shared_ptr<AsyncTask> AsyncTask::create(std::function<void()> function)
{
    return std::shared_ptr<AsyncTask>(new AsyncTask(std::move(function)));
}

void AsyncTask::schedule_now()
{
    boost::asio::post(m_strand, [this, self = shared_from_this()]() {
        m_timer.cancel();
        if (m_function) {
            m_function();
        }
    });
}

void AsyncTask::schedule_after(std::chrono::steady_clock::duration delay)
{
    boost::asio::post(m_strand, [this, delay, self = shared_from_this()]() {
        if (!m_function) {
            return;
        }

        m_timer.expires_after(delay);
        m_timer.async_wait(boost::asio::bind_executor(m_strand, [this, self = shared_from_this()](const boost::system::error_code& ec) {
            if (!ec && m_function) {
                m_function();
            }
        }));
    });
}

void AsyncTask::cancel()
{
    boost::asio::post(m_strand, [this, self = shared_from_this()]() { m_timer.cancel(); });
}

void AsyncTask::destroy()
{
    boost::asio::dispatch(m_strand, std::packaged_task<void()>{[this, self = shared_from_this()]() {
                              m_function = nullptr;
                              m_timer.cancel();
                          }})
        .get();
}

AsyncTask::AsyncTask(std::function<void()> function)
 : m_io_context(IoContextHolder::get()),
   m_strand(m_io_context),
   m_timer(m_io_context),
   m_function(std::move(function))
{
}

ReschedulableTask::ReschedulableTask(BackgroundScheduler& scheduler, std::function<void()> function)
{
    m_state             = std::make_unique<InternalState>(&scheduler, std::move(function));
    m_state->async_task = AsyncTask::create([state = m_state.get()]() {
        std::unique_lock lock{state->mutex};
        if (++state->pending_invocations == 1 && !state->current_invocation) {
            state->current_invocation = state->scheduler
                                            ->run_task([state]() {
                                                std::unique_lock lock{state->mutex};
                                                while (state->pending_invocations > 0) {
                                                    --state->pending_invocations;
                                                    lock.unlock();
                                                    state->function();
                                                    lock.lock();
                                                }
                                                state->current_invocation = std::nullopt;
                                            })
                                            .share();
        }
    });
}

ReschedulableTask::ReschedulableTask(ReschedulableTask&& other)
{
    destroy();
    m_state = std::move(other.m_state);
}

ReschedulableTask::~ReschedulableTask()
{
    destroy();
}

ReschedulableTask& ReschedulableTask::operator=(ReschedulableTask&& other)
{
    if (this != &other) {
        destroy();
        m_state = std::move(other.m_state);
    }
    return *this;
}

bool ReschedulableTask::valid() const
{
    return bool{m_state};
}

void ReschedulableTask::schedule_now()
{
    if (!m_state) {
        return;
    }

    std::unique_lock lock{m_state->mutex};
    if (m_state->async_task && m_state->pending_invocations != -1) {
        m_state->async_task->schedule_now();
    }
}

void ReschedulableTask::schedule_after(std::chrono::steady_clock::duration delay)
{
    if (!m_state) {
        return;
    }

    std::unique_lock lock{m_state->mutex};
    if (m_state->async_task && m_state->pending_invocations != -1) {
        m_state->async_task->schedule_after(delay);
    }
}

void ReschedulableTask::cancel()
{
    if (!m_state) {
        return;
    }

    std::unique_lock lock{m_state->mutex};
    if (m_state->async_task) {
        m_state->async_task->cancel();
    }
}

void ReschedulableTask::destroy()
{
    if (!m_state) {
        return;
    }

    std::unique_lock lock{m_state->mutex};
    if (!m_state->async_task) {
        return;
    }

    m_state->async_task->destroy();
    m_state->pending_invocations = -1;
    if (m_state->current_invocation) {
        std::shared_future<void> current_invocation = *m_state->current_invocation;
        lock.unlock();
        current_invocation.get();
        lock.lock();
        m_state->async_task = nullptr;
    }
}

}  // namespace hareflow::detail