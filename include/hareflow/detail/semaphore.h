#pragma once

#include <cstdint>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace hareflow::detail {

class SemaphoreDestroyedException : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class Semaphore
{
public:
    class Permit
    {
    public:
        Permit() : m_semaphore(nullptr)
        {
        }
        Permit(const Permit&) = delete;
        Permit(Permit&& other) : m_semaphore(other.m_semaphore)
        {
            other.m_semaphore = nullptr;
        }
        ~Permit()
        {
            release();
        }

        Permit& operator=(const Permit&) = delete;
        Permit& operator=(Permit&& other)
        {
            if (this != &other) {
                release();
                m_semaphore       = other.m_semaphore;
                other.m_semaphore = nullptr;
            }
            return *this;
        }

    private:
        friend class Semaphore;

        Permit(Semaphore& semaphore) : m_semaphore(&semaphore)
        {
        }

        void release()
        {
            if (m_semaphore != nullptr) {
                m_semaphore->release();
                m_semaphore = nullptr;
            }
        }

        Semaphore* m_semaphore;
    };

    Semaphore(std::uint32_t desired) : m_lock(), m_released(), m_available(desired), m_destroyed(false)
    {
    }

    Permit acquire()
    {
        std::unique_lock lock{m_lock};
        m_released.wait(lock, [this] { return m_available > 0 || m_destroyed; });
        if (m_destroyed) {
            throw SemaphoreDestroyedException{"Semaphore was destroyed"};
        }
        --m_available;
        return Permit{*this};
    }

    std::optional<Permit> try_acquire_for(std::chrono::milliseconds duration)
    {
        std::unique_lock lock{m_lock};
        bool             acquired = m_released.wait_for(lock, duration, [this] { return m_available > 0 || m_destroyed; });
        if (m_destroyed) {
            throw SemaphoreDestroyedException{"Semaphore was destroyed"};
        }
        if (!acquired) {
            return std::nullopt;
        }
        --m_available;
        return Permit{*this};
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
    void release()
    {
        {
            std::unique_lock lock{m_lock};
            ++m_available;
        }
        m_released.notify_all();
    }

    std::mutex              m_lock;
    std::condition_variable m_released;
    std::uint32_t           m_available;
    bool                    m_destroyed;
};

}  // namespace hareflow::detail
