#include "hareflow/detail/connection.h"

#include "hareflow/exceptions.h"
#include "hareflow/io_context_holder.h"
#include "hareflow/logging.h"

namespace {

const std::uint8_t MAX_POOL_SIZE = 32;

}

namespace hareflow::detail {

namespace io = boost::asio;
using io::ip::tcp;
namespace ssl = io::ssl;

std::shared_ptr<Connection> Connection::create(std::string host, std::uint16_t port, bool use_ssl, bool verify_host)
{
    return std::shared_ptr<Connection>(new Connection(std::move(host), port, use_ssl, verify_host));
}

void Connection::connect()
{
    try {
        tcp::resolver resolver(m_io_context);
        auto          endpoints = resolver.resolve(m_host, std::to_string(m_port));
        io::connect(m_socket, endpoints);
        m_socket.set_option(tcp::no_delay(true));
        m_socket.set_option(tcp_socket::keep_alive(true));
        if (m_ssl) {
            m_ssl->stream().handshake(ssl_stream::client);
        }
        m_connection_failed = false;
    } catch (const boost::system::system_error& e) {
        throw IOException(e.what());
    }

    start_async_read_chain();
}

void Connection::set_writer_idle_handler(std::chrono::seconds timeout, std::function<void()> handler)
{
    io::post(m_strand, std::packaged_task<void()>{[this, timeout, handler = std::move(handler), self = shared_from_this()]() {
                 m_writer_idle_timeout = timeout;
                 m_writer_idle_handler = handler;
                 start_writer_idle_monitoring();
             }})
        .get();
}

void Connection::set_reader_idle_handler(std::chrono::seconds timeout, std::function<void()> handler)
{
    io::post(m_strand, std::packaged_task<void()>{[this, timeout, handler = std::move(handler), self = shared_from_this()]() {
                 m_reader_idle_timeout = timeout;
                 m_reader_idle_handler = handler;
                 start_reader_idle_monitoring();
             }})
        .get();
}

void Connection::shutdown()
{
    boost::system::error_code code;
    m_socket.shutdown(tcp::socket::shutdown_both, code);
    m_socket.close(code);

    io::dispatch(m_strand, std::packaged_task<void()>{[this, self = shared_from_this()]() {
                     m_writer_idle_handler = nullptr;
                     m_reader_idle_handler = nullptr;
                     m_writer_idle_timer.cancel();
                     m_reader_idle_timer.cancel();
                 }})
        .get();
}

BinaryBuffer Connection::allocate_buffer(std::size_t size)
{
    BinaryBuffer buffer;
    {
        std::unique_lock lock(m_buffer_pool_mutex);
        if (!m_buffer_pool.empty()) {
            buffer = std::move(m_buffer_pool.front());
            m_buffer_pool.pop_front();
        }
    }
    buffer.reserve(size);
    return buffer;
}

void Connection::release_buffer(BinaryBuffer buffer)
{
    buffer.clear();
    std::unique_lock lock(m_buffer_pool_mutex);
    if (m_buffer_pool.size() < MAX_POOL_SIZE) {
        m_buffer_pool.emplace_back(std::move(buffer));
    }
}

std::future<void> Connection::write(BinaryBuffer buffer)
{
    auto entry = std::make_shared<OutboxEntry>(std::move(buffer));
    io::post(m_strand, [this, entry, self = shared_from_this()]() {
        m_outbox.emplace_back(std::move(entry));
        if (m_outbox.size() == 1) {
            send_outbox();
        }
    });
    return entry->get_future();
}

std::future<BinaryBuffer> Connection::read()
{
    auto pending_read = std::make_shared<std::promise<BinaryBuffer>>();
    io::post(m_strand, [this, pending_read, self = shared_from_this()]() {
        if (m_pending_read != nullptr) {
            pending_read->set_exception(std::make_exception_ptr(IOException("Illegal state: can not have concurrent reads on connection")));
            return;
        }

        if (!m_inbox.empty()) {
            pending_read->set_value(std::move(m_inbox.front()));
            m_inbox.pop_front();
        } else if (m_connection_failed) {
            pending_read->set_exception(std::make_exception_ptr(IOException("Connection failed unexpectedly")));
        } else {
            m_pending_read = std::move(pending_read);
        }
    });
    return pending_read->get_future();
}

Connection::Connection(std::string host, std::uint16_t port, bool use_ssl, bool verify_host)
 : m_io_context(IoContextHolder::get()),
   m_strand(m_io_context),
   m_socket(m_io_context),
   m_ssl(),
   m_host(std::move(host)),
   m_port(port),
   m_connection_failed(true),
   m_outbox(),
   m_inbox(),
   m_pending_read(),
   m_writer_idle_timeout(),
   m_writer_idle_timer(m_io_context),
   m_writer_idle_handler(),
   m_reader_idle_timeout(),
   m_reader_idle_timer(m_io_context),
   m_reader_idle_handler(),
   m_buffer_pool_mutex(),
   m_buffer_pool()
{
    if (use_ssl) {
        m_ssl = SslAdapter(m_socket, m_host, verify_host);
    }
}

void Connection::send_outbox()
{
    std::size_t nb_buffers = m_outbox.size();
    auto        callback   = [this, nb_buffers, self = shared_from_this()](const boost::system::error_code& ec, std::size_t) mutable {
        while (nb_buffers > 0) {
            if (!ec) {
                m_outbox.front()->set_success();
            } else {
                m_outbox.front()->set_failure(IOException(ec.message()));
            }
            release_buffer(m_outbox.front()->extract_buffer());
            m_outbox.pop_front();
            --nb_buffers;
        }
        start_writer_idle_monitoring();

        if (!m_outbox.empty()) {
            send_outbox();
        }
    };

    std::vector<boost::asio::const_buffer> buffers;
    buffers.reserve(nb_buffers);
    for (const auto& entry : m_outbox) {
        buffers.emplace_back(entry->asio_buffer());
    }

    if (m_ssl) {
        io::async_write(m_ssl->stream(), buffers, io::bind_executor(m_strand, callback));
    } else {
        io::async_write(m_socket, buffers, io::bind_executor(m_strand, callback));
    }
}

void Connection::start_async_read_chain()
{
    auto buffer = std::make_shared<BinaryBuffer>(allocate_buffer(sizeof(std::uint32_t)));
    buffer->resize(sizeof(std::uint32_t));

    auto callback = [this, buffer, self = shared_from_this()](const boost::system::error_code& ec, std::size_t) {
        if (ec) {
            if (m_pending_read) {
                m_pending_read->set_exception(std::make_exception_ptr(IOException(ec.message())));
                m_pending_read = nullptr;
            }
            m_connection_failed = true;
            return;
        }
        start_reader_idle_monitoring();

        std::uint32_t size = buffer->read_uint();
        buffer->resize(sizeof(std::uint32_t) + size);

        auto post_to_inbox = [this, buffer, self = shared_from_this()](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                if (m_pending_read) {
                    m_pending_read->set_exception(std::make_exception_ptr(IOException(ec.message())));
                    m_pending_read = nullptr;
                }
                m_connection_failed = true;
                return;
            }
            start_reader_idle_monitoring();

            if (m_pending_read) {
                m_pending_read->set_value(std::move(*buffer));
                m_pending_read = nullptr;
            } else {
                m_inbox.emplace_back(std::move(*buffer));
            }

            start_async_read_chain();
        };

        if (m_ssl) {
            io::async_read(m_ssl->stream(), buffer->as_asio_buffer() + sizeof(std::uint32_t), io::bind_executor(m_strand, post_to_inbox));
        } else {
            io::async_read(m_socket, buffer->as_asio_buffer() + sizeof(std::uint32_t), io::bind_executor(m_strand, post_to_inbox));
        }
    };

    if (m_ssl) {
        io::async_read(m_ssl->stream(), buffer->as_asio_buffer(), io::bind_executor(m_strand, callback));
    } else {
        io::async_read(m_socket, buffer->as_asio_buffer(), io::bind_executor(m_strand, callback));
    }
}

void Connection::start_writer_idle_monitoring()
{
    if (!m_writer_idle_handler) {
        return;
    }
    m_writer_idle_timer.expires_after(m_writer_idle_timeout);
    m_writer_idle_timer.async_wait(io::bind_executor(m_strand, [this, self = shared_from_this()](const boost::system::error_code& ec) {
        if (!ec && m_writer_idle_handler) {
            m_writer_idle_handler();
        }
    }));
}

void Connection::start_reader_idle_monitoring()
{
    if (!m_reader_idle_handler) {
        return;
    }
    m_reader_idle_timer.expires_after(m_reader_idle_timeout);
    m_reader_idle_timer.async_wait(io::bind_executor(m_strand, [this, self = shared_from_this()](const boost::system::error_code& ec) {
        if (!ec && m_reader_idle_handler) {
            m_reader_idle_handler();
        }
    }));
}

Connection::SslAdapter::SslAdapter(tcp_socket& wrapped_socket, const std::string& host, bool verify_host) : m_context(ssl::context::tls_client), m_stream()
{
    m_context.set_options(ssl_context::default_workarounds | ssl_context::no_tlsv1 | ssl_context::no_tlsv1_1);
    if (verify_host) {
        m_context.set_default_verify_paths();
        m_context.set_verify_mode(ssl::verify_peer);
#if BOOST_VERSION >= 108700
        m_context.set_verify_callback(ssl::host_name_verification(host));
#else
        m_context.set_verify_callback(ssl::rfc2818_verification(host));
#endif
    }
    m_stream = std::make_unique<ssl_stream>(wrapped_socket, m_context);
}

Connection::ssl_stream& Connection::SslAdapter::stream()
{
    return *m_stream;
}

}  // namespace hareflow::detail