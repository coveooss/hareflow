#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "hareflow.h"
#include "hareflow/detail/binary_buffer.h"
#include "hareflow/detail/commands.h"

namespace {

namespace io = boost::asio;
using hareflow::detail::BinaryBuffer;
using hareflow::detail::CommandKey;
using io::ip::tcp;

// Long enough that any test waiting for it would trip the hang detection below
constexpr std::chrono::seconds rpc_timeout{30};
constexpr std::chrono::seconds hang_timeout{10};

[[noreturn]] void abort_on_hang(const char* what)
{
    ADD_FAILURE() << what << " did not complete";
    std::cerr << "\nFATAL: " << what << " did not complete after waiting " << hang_timeout.count()
              << "s.\nExiting immediately: the client's threads may be wedged and teardown could block forever joining them.\n";
    std::cout.flush();
    std::cerr.flush();
    std::fflush(nullptr);
    std::_Exit(EXIT_FAILURE);
}

template<typename Func> auto complete_or_abort(const char* what, Func&& func)
{
    std::packaged_task<std::invoke_result_t<Func>()> task{std::forward<Func>(func)};
    auto                                             future = task.get_future();
    std::thread                                      thread{std::move(task)};
    if (future.wait_for(hang_timeout) != std::future_status::ready) {
        abort_on_hang(what);
    }
    thread.join();
    return future.get();
}

// Broker that completes the connection handshake with a 1 second heartbeat and answers close requests like RabbitMQ, by responding then
// immediately closing the connection. It never sends anything else unless asked to, so the client's reader idle timeout fires after 2 seconds.
class FakeBroker
{
public:
    // drop_connection_on: closes the connection without responding upon receiving this command from the client
    explicit FakeBroker(std::optional<CommandKey> drop_connection_on = std::nullopt)
     : m_drop_connection_on{drop_connection_on},
       m_responding{true},
       m_io_context{},
       m_acceptor{m_io_context, tcp::endpoint{io::ip::address_v4::loopback(), 0}},
       m_socket{m_io_context},
       m_frame{},
       m_thread{}
    {
        m_acceptor.async_accept(m_socket, [this](const boost::system::error_code& ec) {
            if (!ec) {
                read_frame();
            }
        });
        m_thread = std::thread([this] { m_io_context.run(); });
    }

    ~FakeBroker()
    {
        m_io_context.stop();
        m_thread.join();
    }

    std::uint16_t port() const
    {
        return m_acceptor.local_endpoint().port();
    }

    void send_close()
    {
        io::post(m_io_context, [this] {
            send([](BinaryBuffer& buffer) {
                buffer.write_ushort(static_cast<std::uint16_t>(CommandKey::Close));
                buffer.write_ushort(1);
                buffer.write_uint(1);  // correlation id
                buffer.write_ushort(static_cast<std::uint16_t>(hareflow::ResponseCode::Ok));
                buffer.write_string("Closed by fake broker");
            });
        });
    }

    void drop_connection()
    {
        io::post(m_io_context, [this] { close_connection(); });
    }

    // Ignores every command received from the client from now on
    void stop_responding()
    {
        m_responding = false;
    }

private:
    void read_frame()
    {
        m_frame.clear();
        m_frame.resize(sizeof(std::uint32_t));
        io::async_read(m_socket, m_frame.as_asio_buffer(), [this](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                return;
            }
            m_frame.resize(sizeof(std::uint32_t) + m_frame.read_uint());
            io::async_read(m_socket, m_frame.as_asio_buffer() + sizeof(std::uint32_t), [this](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    return;
                }
                if (handle_frame()) {
                    read_frame();
                }
            });
        });
    }

    // Returns false once the connection is closed
    bool handle_frame()
    {
        if (!m_responding) {
            return true;
        }

        auto key = static_cast<CommandKey>(m_frame.read_ushort());
        m_frame.read_ushort();  // version

        if (key == m_drop_connection_on) {
            close_connection();
            return false;
        }

        switch (key) {
            case CommandKey::PeerProperties:
            case CommandKey::Open:
                respond(key, [](BinaryBuffer& buffer) { buffer.write_int(0); });  // no properties
                break;
            case CommandKey::SaslHandshake:
                respond(key, [](BinaryBuffer& buffer) {
                    buffer.write_int(1);
                    buffer.write_string("PLAIN");
                });
                break;
            case CommandKey::SaslAuthenticate:
                respond(key, [](BinaryBuffer& /*buffer*/) {});
                send([](BinaryBuffer& buffer) {
                    buffer.write_ushort(static_cast<std::uint16_t>(CommandKey::Tune));
                    buffer.write_ushort(1);
                    buffer.write_uint(0);  // max frame size, unlimited
                    buffer.write_uint(1);  // heartbeat, in seconds
                });
                break;
            case CommandKey::Close:
                respond(key, [](BinaryBuffer& /*buffer*/) {});
                close_connection();
                return false;
            default:
                // The client's tune response, heartbeats and close responses are ignored
                break;
        }
        return true;
    }

    void close_connection()
    {
        boost::system::error_code ec;
        m_socket.close(ec);
    }

    template<typename WriteBody> void respond(CommandKey key, WriteBody&& write_body)
    {
        std::uint32_t correlation_id = m_frame.read_uint();
        send([&](BinaryBuffer& buffer) {
            buffer.write_ushort(0x8000 | static_cast<std::uint16_t>(key));
            buffer.write_ushort(1);
            buffer.write_uint(correlation_id);
            buffer.write_ushort(static_cast<std::uint16_t>(hareflow::ResponseCode::Ok));
            write_body(buffer);
        });
    }

    template<typename WriteCommand> void send(WriteCommand&& write_command)
    {
        BinaryBuffer command;
        write_command(command);

        BinaryBuffer frame;
        frame.write_uint(static_cast<std::uint32_t>(command.size()));
        frame.write_bytes(command.as_asio_buffer());

        boost::system::error_code ec;
        io::write(m_socket, frame.as_asio_buffer(), ec);
    }

    std::optional<CommandKey> m_drop_connection_on;
    std::atomic<bool>         m_responding;
    io::io_context            m_io_context;
    tcp::acceptor             m_acceptor;
    tcp::socket               m_socket;
    BinaryBuffer              m_frame;
    std::thread               m_thread;
};

}  // namespace

class ClientTest : public testing::Test
{
protected:
    void create_client(const FakeBroker& broker)
    {
        m_client = hareflow::Client::create(hareflow::ClientParameters{}
                                                .with_host("127.0.0.1")
                                                .with_port(broker.port())
                                                .with_requested_heartbeat(std::chrono::seconds{1})
                                                .with_rpc_timeout(rpc_timeout)
                                                .with_shutdown_listener(std::bind(&ClientTest::handle_shutdown, this, std::placeholders::_1)));
    }

    hareflow::ShutdownReason wait_for_shutdown_reason(const char* what)
    {
        if (m_shutdown_reason_future.wait_for(hang_timeout) != std::future_status::ready) {
            abort_on_hang(what);
        }
        return m_shutdown_reason_future.get();
    }

    bool shutdown_listener_called() const
    {
        return m_shutdown_reason_future.wait_for(std::chrono::seconds::zero()) == std::future_status::ready;
    }

    // Like ConsumerImpl::handle_shutdown, drops what may be the last reference to the client from within its shutdown listener.
    void handle_shutdown(hareflow::ShutdownReason reason)
    {
        if (reason != hareflow::ShutdownReason::ClientInitiated) {
            m_client = nullptr;
        }
        m_shutdown_reason.set_value(reason);
    }

    hareflow::ClientPtr                    m_client;
    std::promise<hareflow::ShutdownReason> m_shutdown_reason;
    std::future<hareflow::ShutdownReason>  m_shutdown_reason_future = m_shutdown_reason.get_future();
};

TEST_F(ClientTest, ReleaseFromShutdownListenerOnHeartbeatTimeout)
{
    FakeBroker broker;
    create_client(broker);
    m_client->start();

    EXPECT_EQ(wait_for_shutdown_reason("Releasing the client from its shutdown listener after a heartbeat timeout"),
              hareflow::ShutdownReason::HeartbeatTimeout);
}

TEST_F(ClientTest, ReleaseFromShutdownListenerOnServerClose)
{
    FakeBroker broker;
    create_client(broker);
    m_client->start();
    broker.send_close();

    EXPECT_EQ(wait_for_shutdown_reason("Releasing the client from its shutdown listener after a server close"), hareflow::ShutdownReason::ServerInitiated);
}

TEST_F(ClientTest, ReleaseFromShutdownListenerOnConnectionFailure)
{
    FakeBroker broker;
    create_client(broker);
    m_client->start();
    broker.drop_connection();

    EXPECT_EQ(wait_for_shutdown_reason("Releasing the client from its shutdown listener after a connection failure"), hareflow::ShutdownReason::Unknown);
}

TEST_F(ClientTest, StopDoesNotWaitForTimeoutWhenConnectionFailsDuringCloseHandshake)
{
    FakeBroker broker{CommandKey::Close};
    create_client(broker);
    m_client->start();

    complete_or_abort("Stopping the client while the connection fails", [this] { m_client->stop(); });
    EXPECT_EQ(wait_for_shutdown_reason("Notifying the shutdown listener"), hareflow::ShutdownReason::ClientInitiated);
}

TEST_F(ClientTest, StopCompletesCloseHandshakeCleanly)
{
    std::mutex               warnings_mutex;
    std::vector<std::string> warnings;
    hareflow::Logger::to_custom_handler([&](hareflow::LoggingLevel level, std::string message) {
        std::lock_guard lock{warnings_mutex};
        if (level >= hareflow::LoggingLevel::Warn) {
            warnings.emplace_back(std::move(message));
        }
    });

    FakeBroker broker;
    create_client(broker);
    m_client->start();

    complete_or_abort("Stopping the client", [this] { m_client->stop(); });
    hareflow::Logger::disable();

    EXPECT_EQ(wait_for_shutdown_reason("Notifying the shutdown listener"), hareflow::ShutdownReason::ClientInitiated);
    EXPECT_TRUE(warnings.empty()) << "Close response should have been processed, but got warning: " << warnings.front();
}

TEST_F(ClientTest, StopDoesNotWaitForTimeoutWhenHeartbeatTimesOutDuringCloseHandshake)
{
    FakeBroker broker;
    create_client(broker);
    m_client->start();
    broker.stop_responding();

    complete_or_abort("Stopping the client while the broker is unresponsive", [this] { m_client->stop(); });
    EXPECT_EQ(wait_for_shutdown_reason("Notifying the shutdown listener"), hareflow::ShutdownReason::ClientInitiated);
}

TEST_F(ClientTest, StartDoesNotWaitForTimeoutWhenConnectionFails)
{
    FakeBroker broker{CommandKey::Open};
    create_client(broker);

    complete_or_abort("Starting the client while the connection fails", [this] { EXPECT_THROW(m_client->start(), hareflow::IOException); });

    m_client = nullptr;
    EXPECT_FALSE(shutdown_listener_called()) << "Shutdown listener must not be called for a client that never started";
}

TEST_F(ClientTest, StopBeforeStart)
{
    FakeBroker broker;
    create_client(broker);
    m_client->stop();

    EXPECT_THROW(m_client->start(), hareflow::StreamException);
    m_client = nullptr;
    EXPECT_FALSE(shutdown_listener_called()) << "Shutdown listener must not be called for a client that never started";
}
