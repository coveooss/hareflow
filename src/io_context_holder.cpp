#include "hareflow/io_context_holder.h"

namespace hareflow {

std::function<boost::asio::io_context&()> IoContextHolder::s_io_context_func{&DefaultIoContext::get};
std::unique_ptr<DefaultIoContext>         DefaultIoContext::s_instance;

DefaultIoContext::~DefaultIoContext()
{
    m_work_guard.reset();
    for (auto& thread : m_io_threads) {
        thread.join();
    }
}

void DefaultIoContext::initialize(std::uint32_t nb_io_threads)
{
    static std::atomic<bool> initialized{false};
    static std::mutex        initialize_mutex;
    if (!initialized) {
        std::unique_lock lock{initialize_mutex};
        if (s_instance == nullptr) {
            s_instance  = std::unique_ptr<DefaultIoContext>{new DefaultIoContext{nb_io_threads}};
            initialized = true;
        }
    }
}

boost::asio::io_context& DefaultIoContext::get()
{
    initialize();
    return DefaultIoContext::s_instance->m_io_context;
}

DefaultIoContext::DefaultIoContext(std::uint32_t nb_io_threads) : m_io_context{}, m_work_guard{m_io_context.get_executor()}, m_io_threads{}
{
    if (nb_io_threads == 0) {
        nb_io_threads = std::max(1u, std::thread::hardware_concurrency());
    }
    for (std::uint32_t i = 0; i < nb_io_threads; ++i) {
        m_io_threads.emplace_back([this]() { m_io_context.run(); });
    }
}

}  // namespace hareflow