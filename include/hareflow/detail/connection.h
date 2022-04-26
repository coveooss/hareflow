#pragma once

#include <chrono>
#include <deque>
#include <memory>

#ifdef _WIN32
#    pragma warning(push)
#    pragma warning(disable : 4996)
#endif

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#ifdef _WIN32
#    pragma warning(pop)
#endif

#include "hareflow/detail/binary_buffer.h"

namespace hareflow::detail {

class Connection : public std::enable_shared_from_this<Connection>
{
public:
    static std::shared_ptr<Connection> create(std::string host, std::uint16_t port, bool use_ssl, bool verify_host);

    void connect();
    void shutdown();

    void                      set_writer_idle_handler(std::chrono::seconds timeout, std::function<void()> handler);
    void                      set_reader_idle_handler(std::chrono::seconds timeout, std::function<void()> handler);
    BinaryBuffer              allocate_buffer(std::size_t size);
    void                      release_buffer(BinaryBuffer buffer);
    std::future<void>         write(BinaryBuffer buffer);
    std::future<BinaryBuffer> read();

private:
    using tcp_socket = boost::asio::ip::tcp::socket;
    using ssl_stream = boost::asio::ssl::stream<tcp_socket&>;
    class OutboxEntry
    {
    public:
        OutboxEntry(BinaryBuffer buffer) : m_buffer(std::move(buffer)), m_sent_promise()
        {
        }

        void set_success()
        {
            m_sent_promise.set_value();
        }

        void set_failure(IOException e)
        {
            m_sent_promise.set_exception(std::make_exception_ptr(e));
        }

        std::future<void> get_future()
        {
            return m_sent_promise.get_future();
        }

        boost::asio::const_buffer asio_buffer()
        {
            return m_buffer.as_asio_buffer();
        }

        BinaryBuffer&& extract_buffer()
        {
            return std::move(m_buffer);
        }

    private:
        BinaryBuffer       m_buffer;
        std::promise<void> m_sent_promise;
    };

    Connection(std::string host, std::uint16_t port, bool use_ssl, bool verify_host);
    void send_outbox();
    void start_async_read_chain();
    void start_writer_idle_monitoring();
    void start_reader_idle_monitoring();

    boost::asio::io_context&        m_io_context;
    boost::asio::io_context::strand m_strand;
    tcp_socket                      m_socket;
    std::unique_ptr<ssl_stream>     m_ssl_stream;
    std::string                     m_host;
    std::uint16_t                   m_port;
    bool                            m_connection_failed;

    std::deque<std::shared_ptr<OutboxEntry>> m_outbox;
    std::deque<BinaryBuffer>                 m_inbox;

    std::shared_ptr<std::promise<BinaryBuffer>> m_pending_read;

    std::chrono::seconds      m_writer_idle_timeout;
    boost::asio::steady_timer m_writer_idle_timer;
    std::function<void()>     m_writer_idle_handler;
    std::chrono::seconds      m_reader_idle_timeout;
    boost::asio::steady_timer m_reader_idle_timer;
    std::function<void()>     m_reader_idle_handler;

    std::mutex               m_buffer_pool_mutex;
    std::deque<BinaryBuffer> m_buffer_pool;
};

}  // namespace hareflow::detail