#include <chrono>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <thread>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "hareflow.h"
#include "hareflow/detail/binary_buffer.h"
#include "hareflow/detail/commands.h"

namespace {

namespace io = boost::asio;
using io::ip::tcp;
using hareflow::detail::BinaryBuffer;
using hareflow::detail::CommandKey;

constexpr std::chrono::seconds deadlock_timeout{10};

[[noreturn]] void abort_on_deadlock(const char* what)
{
    ADD_FAILURE() << what << " deadlocked";
    std::cerr << "\nFATAL: " << what << " deadlocked after waiting " << deadlock_timeout.count()
              << "s.\nExiting immediately: the client's I/O thread is wedged and teardown would block forever joining it.\n";
    std::cout.flush();
    std::cerr.flush();
    std::fflush(nullptr);
    std::_Exit(EXIT_FAILURE);
}

// Broker that completes the connection handshake with a 1 second heartbeat, then never sends anything again,
// so the client's reader idle timeout fires.
class SilentBroker
{
public:
    SilentBroker() : m_io_context{}, m_acceptor{m_io_context, tcp::endpoint{io::ip::address_v4::loopback(), 0}}, m_socket{m_io_context}, m_frame{}, m_thread{}
    {
        m_acceptor.async_accept(m_socket, [this](const boost::system::error_code& ec) {
            if (!ec) {
                read_frame();
            }
        });
        m_thread = std::thread([this] { m_io_context.run(); });
    }

    ~SilentBroker()
    {
        m_io_context.stop();
        m_thread.join();
    }

    std::uint16_t port() const
    {
        return m_acceptor.local_endpoint().port();
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
                handle_frame();
                read_frame();
            });
        });
    }

    void handle_frame()
    {
        auto key = static_cast<CommandKey>(m_frame.read_ushort());
        m_frame.read_ushort();  // version

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
            default:
                // The client's tune response and heartbeats need no answer
                break;
        }
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

    io::io_context m_io_context;
    tcp::acceptor  m_acceptor;
    tcp::socket    m_socket;
    BinaryBuffer   m_frame;
    std::thread    m_thread;
};

}  // namespace

class ClientTest : public testing::Test
{
protected:
    hareflow::ShutdownListener shutdown_listener()
    {
        return std::bind(&ClientTest::handle_shutdown, this, std::placeholders::_1);
    }

    // Like ConsumerImpl::handle_shutdown, drops what may be the last reference to the client from within its shutdown listener.
    void handle_shutdown(hareflow::ShutdownReason reason)
    {
        m_client = nullptr;
        m_shutdown_reason.set_value(reason);
    }

    hareflow::ClientPtr                    m_client;
    std::promise<hareflow::ShutdownReason> m_shutdown_reason;
};

TEST_F(ClientTest, ReleaseFromShutdownListenerOnHeartbeatTimeout)
{
    SilentBroker broker;

    m_client = hareflow::Client::create(hareflow::ClientParameters{}
                                            .with_host("127.0.0.1")
                                            .with_port(broker.port())
                                            .with_requested_heartbeat(std::chrono::seconds{1})
                                            .with_shutdown_listener(shutdown_listener()));
    m_client->start();

    std::future<hareflow::ShutdownReason> shutdown_reason = m_shutdown_reason.get_future();
    if (shutdown_reason.wait_for(deadlock_timeout) != std::future_status::ready) {
        abort_on_deadlock("Releasing the client from its shutdown listener after a heartbeat timeout");
    }
    EXPECT_EQ(shutdown_reason.get(), hareflow::ShutdownReason::HeartbeatTimeout);
}
