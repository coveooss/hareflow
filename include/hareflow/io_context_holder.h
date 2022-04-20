#pragma once

#include <cstdint>

#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "hareflow/export.h"

namespace hareflow {

class HAREFLOW_EXPORT IoContextHolder
{
public:
    static void set(std::function<boost::asio::io_context&()> io_context_func)
    {
        s_io_context_func = std::move(io_context_func);
    }

    static boost::asio::io_context& get()
    {
        return s_io_context_func();
    }

private:
    static std::function<boost::asio::io_context&()> s_io_context_func;
};

class HAREFLOW_EXPORT DefaultIoContext
{
public:
    ~DefaultIoContext();

    static void                     initialize(std::uint32_t nb_io_threads = 0);
    static boost::asio::io_context& get();

private:
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    DefaultIoContext(std::uint32_t nb_io_threads);

    boost::asio::io_context  m_io_context;
    WorkGuard                m_work_guard;
    std::vector<std::thread> m_io_threads;

    static std::unique_ptr<DefaultIoContext> s_instance;
};

}  // namespace hareflow