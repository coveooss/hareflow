#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "hareflow/detail/background_scheduler.h"
#include "hareflow/detail/reschedulable_task.h"

namespace {

constexpr std::chrono::seconds deadlock_timeout{10};

[[noreturn]] void abort_on_deadlock(const char* what, int iteration)
{
    ADD_FAILURE() << what << " deadlocked at iteration " << iteration;
    std::cerr << "\nFATAL: " << what << " deadlocked at iteration " << iteration << " after waiting " << deadlock_timeout.count()
              << "s.\nExiting immediately: threads are wedged on the task mutex and teardown would block forever joining them.\n";
    std::cout.flush();
    std::cerr.flush();
    std::fflush(nullptr);
    std::_Exit(EXIT_FAILURE);
}

// Tasks that outlive the test on purpose. If an assertion about "nothing runs after
// destroy()" fails, the task function may still be executing, so freeing the task would
// turn a clean assertion failure into a crash or a use-after-free.
hareflow::detail::ReschedulableTask& leaked_task(hareflow::detail::BackgroundScheduler& scheduler, std::function<void()> function)
{
    return *new hareflow::detail::ReschedulableTask{scheduler, std::move(function)};
}

}  // namespace

TEST(ReschedulableTaskTest, DestroyDoesNotDeadlockWithConcurrentFiring)
{
    hareflow::detail::BackgroundScheduler scheduler;

    for (int iteration = 0; iteration < 200; ++iteration) {
        hareflow::detail::ReschedulableTask task{scheduler, []() {}};

        auto        cancel = std::make_shared<std::atomic<bool>>(false);
        std::thread spam([&task, cancel]() {
            while (!cancel->load(std::memory_order_relaxed)) {
                task.schedule_now();
            }
        });

        auto              destroyed        = std::make_shared<std::promise<void>>();
        std::future<void> destroyed_future = destroyed->get_future();
        std::thread       destroyer([&task, destroyed]() {
            task.destroy();
            destroyed->set_value();
        });

        if (destroyed_future.wait_for(deadlock_timeout) != std::future_status::ready) {
            abort_on_deadlock("ReschedulableTask::destroy()", iteration);
        }
        cancel->store(true, std::memory_order_relaxed);
        spam.join();
        destroyer.join();
    }
}

TEST(ReschedulableTaskTest, DestroyDoesNotDrainTheBacklog)
{
    constexpr std::chrono::milliseconds work{20};
    constexpr int                       backlog                        = 40;
    constexpr int                       max_invocations_during_destroy = 3;

    hareflow::detail::BackgroundScheduler scheduler;

    auto invocations    = std::make_shared<std::atomic<int>>(0);
    auto backlog_proven = std::make_shared<std::promise<void>>();
    auto proven_set     = std::make_shared<std::atomic<bool>>(false);

    auto& task = leaked_task(scheduler, [invocations, backlog_proven, proven_set, work]() {
        std::this_thread::sleep_for(work);
        if (invocations->fetch_add(1, std::memory_order_acq_rel) + 1 >= 2 && !proven_set->exchange(true)) {
            backlog_proven->set_value();
        }
    });

    for (int i = 0; i < backlog; ++i) {
        task.schedule_now();
    }

    auto backlog_proven_future = backlog_proven->get_future();
    ASSERT_EQ(backlog_proven_future.wait_for(std::chrono::seconds{30}), std::future_status::ready) << "task never built up a backlog";

    const int before = invocations->load(std::memory_order_acquire);

    auto              destroyed        = std::make_shared<std::promise<void>>();
    std::future<void> destroyed_future = destroyed->get_future();
    auto              elapsed_ms       = std::make_shared<std::atomic<long long>>(0);
    std::thread       destroyer([&task, destroyed, elapsed_ms]() {
        auto start = std::chrono::steady_clock::now();
        task.destroy();
        elapsed_ms->store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
        destroyed->set_value();
    });

    if (destroyed_future.wait_for(deadlock_timeout) != std::future_status::ready) {
        abort_on_deadlock("ReschedulableTask::destroy()", 0);
    }
    destroyer.join();

    const int during = invocations->load(std::memory_order_acquire) - before;
    EXPECT_LE(during, max_invocations_during_destroy) << "destroy() ran " << during << " invocations before returning (took " << elapsed_ms->load()
                                                      << "ms); it should stop the task, not drain the " << backlog << " queued invocations";

    const int after_destroy = invocations->load(std::memory_order_acquire);
    task.schedule_now();
    std::this_thread::sleep_for(work * 4);
    EXPECT_EQ(invocations->load(std::memory_order_acquire), after_destroy) << "the task function ran after destroy() returned";
}
TEST(ReschedulableTaskTest, SelfReschedulingTaskStopsOnDestroy)
{
    hareflow::detail::BackgroundScheduler scheduler;

    for (int iteration = 0; iteration < 50; ++iteration) {
        auto invocations = std::make_shared<std::atomic<int>>(0);
        auto started     = std::make_shared<std::promise<void>>();
        auto started_set = std::make_shared<std::atomic<bool>>(false);

        auto* task = new hareflow::detail::ReschedulableTask{};
        *task      = hareflow::detail::ReschedulableTask{scheduler, [task, invocations, started, started_set]() {
                                                        invocations->fetch_add(1, std::memory_order_acq_rel);
                                                        if (!started_set->exchange(true)) {
                                                            started->set_value();
                                                        }
                                                        if (task->valid()) {
                                                            task->schedule_now();
                                                        }
                                                    }};

        task->schedule_now();
        auto started_future = started->get_future();
        ASSERT_EQ(started_future.wait_for(std::chrono::seconds{10}), std::future_status::ready) << "task never fired at iteration " << iteration;

        auto              destroyed        = std::make_shared<std::promise<void>>();
        std::future<void> destroyed_future = destroyed->get_future();
        std::thread       destroyer([task, destroyed]() {
            task->destroy();
            destroyed->set_value();
        });

        if (destroyed_future.wait_for(deadlock_timeout) != std::future_status::ready) {
            abort_on_deadlock("ReschedulableTask::destroy() on a self-rescheduling task", iteration);
        }
        destroyer.join();

        const int after_destroy = invocations->load(std::memory_order_acquire);
        task->schedule_now();
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        ASSERT_EQ(invocations->load(std::memory_order_acquire), after_destroy)
            << "self-rescheduling task kept running after destroy() returned, iteration " << iteration;
    }
}
