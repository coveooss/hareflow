#pragma once

#include <cstdint>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "hareflow/export.h"

namespace hareflow {

namespace detail {

class InternalEnvironment;
using InternalEnvironmentPtr     = std::shared_ptr<InternalEnvironment>;
using InternalEnvironmentWeakPtr = std::weak_ptr<InternalEnvironment>;

}  // namespace detail

class Client;
class ClientParameters;
class Environment;
class Producer;
class Consumer;
class Codec;

using ClientPtr      = std::shared_ptr<Client>;
using EnvironmentPtr = std::shared_ptr<Environment>;
using ProducerPtr    = std::shared_ptr<Producer>;
using ConsumerPtr    = std::shared_ptr<Consumer>;
using CodecPtr       = std::shared_ptr<Codec>;

using Properties = std::map<std::string, std::string>;

struct MessageContext {
    std::uint64_t offset;
    std::int64_t  timestamp;
    Consumer*     consumer;
};

class Message
{
public:
    Message(std::vector<std::uint8_t> body, std::map<std::string, std::string> headers) : m_body{std::move(body)}, m_headers{std::move(headers)}
    {
    }

    const std::vector<std::uint8_t>& body() const
    {
        return m_body;
    }

    const std::map<std::string, std::string>& headers() const
    {
        return m_headers;
    }

private:
    std::vector<std::uint8_t>          m_body;
    std::map<std::string, std::string> m_headers;
};
using MessagePtr = std::shared_ptr<Message>;

enum class ShutdownReason { ClientInitiated, ServerInitiated, HeartbeatTimeout, Unknown };

enum class ResponseCode : std::uint16_t {
    Ok                                = 0x0001,
    StreamDoesNotExist                = 0x0002,
    SubscriptionIdAlreadyExists       = 0x0003,
    SubscriptionIdDoesNotExist        = 0x0004,
    StreamAlreadyExists               = 0x0005,
    StreamNotAvailable                = 0x0006,
    SaslMechanismNotSupported         = 0x0007,
    AuthenticationFailure             = 0x0008,
    SaslError                         = 0x0009,
    SaslChallenge                     = 0x000a,
    SaslAuthenticationFailureLoopback = 0x000b,
    VirtualHostAccessFailure          = 0x000c,
    UnknownFrame                      = 0x000d,
    FrameTooLarge                     = 0x000e,
    InternalError                     = 0x000f,
    AccessRefused                     = 0x0010,
    PreconditionFailed                = 0x0011,
    PublisherDoesNotExist             = 0x0012,
    NoOffset                          = 0x0013
};

enum class ProducerErrorCode : std::uint16_t {
    MessageEnqueuingFailed = 0x4001,
    ProducerNotAvailable   = 0x4002,
    ProducerStopped        = 0x4003,
    ConfirmTimeout         = 0x4004
};

struct ConfirmationStatus {
    std::uint64_t publishing_id;
    MessagePtr    message;
    bool          confirmed;
    std::uint16_t status_code;  // mixin of ResponseCode + ProducerErrorCode
};
using ConfirmationHandler = std::function<void(const ConfirmationStatus&)>;
using MessageHandler      = std::function<void(const MessageContext&, MessagePtr)>;

class AutomaticCursorConfiguration
{
public:
    static constexpr std::uint32_t default_persist_frequency()
    {
        return 10'000;
    }
    static constexpr std::chrono::seconds default_force_persist_delay()
    {
        return std::chrono::seconds{5};
    }

    AutomaticCursorConfiguration() = default;
    AutomaticCursorConfiguration(std::uint32_t persist_frequency, std::chrono::milliseconds force_persist_delay)
     : m_persist_frequency{persist_frequency},
       m_force_persist_delay{force_persist_delay}
    {
    }

    std::uint32_t persist_frequency() const
    {
        return m_persist_frequency;
    }

    std::chrono::milliseconds force_persist_delay() const
    {
        return m_force_persist_delay;
    }

private:
    std::uint32_t             m_persist_frequency   = default_persist_frequency();
    std::chrono::milliseconds m_force_persist_delay = default_force_persist_delay();
};

class HAREFLOW_EXPORT OffsetSpecification
{
public:
    static OffsetSpecification first();
    static OffsetSpecification last();
    static OffsetSpecification next();
    static OffsetSpecification offset(std::uint64_t offset);
    static OffsetSpecification timestamp(std::chrono::system_clock::time_point timestamp);
    static OffsetSpecification timestamp(std::int64_t timestamp);

    std::uint16_t get_type() const;
    std::uint64_t get_offset() const;
    bool          is_offset() const;
    bool          is_timestamp() const;

    bool operator==(const OffsetSpecification&) const;

private:
    OffsetSpecification(std::uint16_t type, std::uint64_t offset);

    std::uint16_t m_type;
    std::uint64_t m_offset;
};

class HAREFLOW_EXPORT Broker
{
public:
    Broker() = default;
    Broker(std::string host, std::uint16_t port);
    Broker(const Broker&) = default;
    Broker(Broker&&)      = default;
    Broker& operator=(const Broker&) = default;
    Broker& operator=(Broker&&) = default;

    const std::string& get_host() const;
    std::uint16_t      get_port() const;

    std::string& get_host();

private:
    std::string   m_host;
    std::uint16_t m_port;
};

class HAREFLOW_EXPORT StreamMetadata
{
public:
    StreamMetadata(std::string stream_name, ResponseCode response_code, Broker leader, std::vector<Broker> replicas);
    StreamMetadata(const StreamMetadata&) = default;
    StreamMetadata(StreamMetadata&&)      = default;
    StreamMetadata& operator=(const StreamMetadata&) = default;
    StreamMetadata& operator=(StreamMetadata&&) = default;

    const std::string&         get_stream_name() const;
    ResponseCode               get_response_code() const;
    const Broker&              get_leader() const;
    const std::vector<Broker>& get_replicas() const;

    std::string&         get_stream_name();
    Broker&              get_leader();
    std::vector<Broker>& get_replicas();

private:
    std::string         m_stream_name;
    ResponseCode        m_response_code;
    Broker              m_leader;
    std::vector<Broker> m_replicas;
};

struct StreamMetadataLess {
    using is_transparent = void;

    bool operator()(const StreamMetadata& left, const StreamMetadata& right) const
    {
        return left.get_stream_name() < right.get_stream_name();
    }

    bool operator()(std::string_view left, const StreamMetadata& right) const
    {
        return left < right.get_stream_name();
    }

    bool operator()(const StreamMetadata& left, std::string_view right) const
    {
        return left.get_stream_name() < right;
    }
};

using StreamMetadataSet = std::set<StreamMetadata, StreamMetadataLess>;

}  // namespace hareflow