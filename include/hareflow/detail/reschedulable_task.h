#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>

#include <boost/asio.hpp>

namespace hareflow::detail {

class BackgroundScheduler;

class AsyncTask : public std::enable_shared_from_this<AsyncTask>
{
public:
    static std::shared_ptr<AsyncTask> create(std::function<void()> function);

    void schedule_now();
    void schedule_after(std::chrono::steady_clock::duration delay);
    void cancel();
    void destroy();

private:
    AsyncTask(std::function<void()> function);

    boost::asio::io_context&        m_io_context;
    boost::asio::io_context::strand m_strand;
    boost::asio::steady_timer       m_timer;
    std::function<void()>           m_function;
};

class ReschedulableTask
{
public:
    ReschedulableTask() = default;
    ReschedulableTask(BackgroundScheduler& scheduler, std::function<void()> function);
    ReschedulableTask(const ReschedulableTask&) = delete;
    ReschedulableTask(ReschedulableTask&& other);
    ~ReschedulableTask();

    ReschedulableTask& operator=(const ReschedulableTask&) = delete;
    ReschedulableTask& operator                            =(ReschedulableTask&& other);

    bool valid() const;
    void schedule_now();
    void schedule_after(std::chrono::steady_clock::duration delay);
    void cancel();
    void destroy();

private:
    struct InternalState {
        InternalState(BackgroundScheduler* scheduler, std::function<void()> function) : scheduler{scheduler}, function{std::move(function)}
        {
        }

        BackgroundScheduler*                    scheduler;
        std::function<void()>                   function;
        std::mutex                              mutex{};
        std::int64_t                            pending_invocations{0};
        std::optional<std::shared_future<void>> current_invocation{};
        std::shared_ptr<AsyncTask>              async_task;
    };
    std::unique_ptr<InternalState> m_state{};
};

}  // namespace hareflow::detail