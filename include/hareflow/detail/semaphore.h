#pragma once

#include <cstdint>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace hareflow::detail {

class SemaphoreDestroyedException : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class Semaphore
{
public:
    Semaphore(std::uint32_t desired) : m_lock(), m_released(), m_available(desired), m_destroyed(false)
    {
    }

    void acquire()
    {
        std::unique_lock lock{m_lock};
        m_released.wait(lock, [this] { return m_available > 0 || m_destroyed; });
        if (m_destroyed) {
            throw SemaphoreDestroyedException{"Semaphore was destroyed"};
        }
        --m_available;
    }

    bool try_acquire_for(std::chrono::milliseconds duration)
    {
        std::unique_lock lock{m_lock};
        bool             acquired = m_released.wait_for(lock, duration, [this] { return m_available > 0 || m_destroyed; });
        if (m_destroyed) {
            throw SemaphoreDestroyedException{"Semaphore was destroyed"};
        }
        if (acquired) {
            --m_available;
        }
        return acquired;
    }

    void release(std::uint32_t count = 1)
    {
        {
            std::unique_lock lock{m_lock};
            m_available += count;
        }
        m_released.notify_all();
    }

    void destroy()
    {
        {
            std::unique_lock lock{m_lock};
            m_destroyed = true;
        }
        m_released.notify_all();
    }

private:
    std::mutex              m_lock;
    std::condition_variable m_released;
    std::uint32_t           m_available;
    bool                    m_destroyed;
};

}  // namespace hareflow::detail
