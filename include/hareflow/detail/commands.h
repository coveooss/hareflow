#pragma once

// Protocol doc: https://github.com/rabbitmq/rabbitmq-server/blob/v3.9.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc

#include "hareflow/detail/binary_buffer.h"
#include "hareflow/detail/internal_types.h"

namespace hareflow::detail {

enum class CommandKey : std::uint16_t {
    DeclarePublisher       = 0x0001,
    Publish                = 0x0002,
    PublishConfirm         = 0x0003,
    PublishError           = 0x0004,
    QueryPublisherSequence = 0x0005,
    DeletePublisher        = 0x0006,
    Subscribe              = 0x0007,
    Deliver                = 0x0008,
    Credit                 = 0x0009,
    StoreOffset            = 0x000a,
    QueryOffset            = 0x000b,
    Unsubscribe            = 0x000c,
    Create                 = 0x000d,
    Delete                 = 0x000e,
    Metadata               = 0x000f,
    MetadataUpdate         = 0x0010,
    PeerProperties         = 0x0011,
    SaslHandshake          = 0x0012,
    SaslAuthenticate       = 0x0013,
    Tune                   = 0x0014,
    Open                   = 0x0015,
    Close                  = 0x0016,
    Heartbeat              = 0x0017,
    Route                  = 0x0018,
    Partitions             = 0x0019
};

class Deserializable
{
public:
    virtual ~Deserializable()                      = default;
    virtual void deserialize(BinaryBuffer& buffer) = 0;
};

class Serializable
{
public:
    virtual ~Serializable()                                   = default;
    virtual std::size_t serialized_size() const               = 0;
    virtual void        serialize(BinaryBuffer& buffer) const = 0;
};

class ClientCommand : public Serializable
{
public:
    ClientCommand(CommandKey key) : m_key(static_cast<std::uint16_t>(key))
    {
    }
    ClientCommand(std::uint16_t key) : m_key(key)
    {
    }
    ClientCommand(const ClientCommand&) = delete;
    ClientCommand& operator=(const ClientCommand&) = delete;

    std::uint16_t get_key() const
    {
        return m_key;
    }

    std::uint16_t get_version() const
    {
        return m_version;
    }

    std::size_t serialized_size() const final
    {
        return header_size() + body_size();
    }

    void serialize(BinaryBuffer& buffer) const final
    {
        write_header(buffer);
        write_body(buffer);
    }

protected:
    virtual std::size_t body_size() const                      = 0;
    virtual void        write_body(BinaryBuffer& buffer) const = 0;

private:
    std::size_t header_size() const;
    void        write_header(BinaryBuffer& buffer) const;

    std::uint16_t m_key;
    std::uint16_t m_version{1};
};

class ClientRequestOrResponse : public Serializable
{
public:
    ClientRequestOrResponse(CommandKey key, std::uint32_t correlation_id) : m_key(static_cast<std::uint16_t>(key)), m_correlation_id(correlation_id)
    {
    }
    ClientRequestOrResponse(std::uint16_t key, std::uint32_t correlation_id) : m_key(key), m_correlation_id(correlation_id)
    {
    }
    ClientRequestOrResponse(const ClientRequestOrResponse&) = delete;
    ClientRequestOrResponse& operator=(const ClientRequestOrResponse&) = delete;

    std::uint16_t get_key() const
    {
        return m_key;
    }

    std::uint16_t get_version() const
    {
        return m_version;
    }

    std::uint32_t get_correlation_id() const
    {
        return m_correlation_id;
    }

    std::size_t serialized_size() const final
    {
        return header_size() + body_size();
    }

    void serialize(BinaryBuffer& buffer) const final
    {
        write_header(buffer);
        write_body(buffer);
    }

protected:
    virtual std::size_t body_size() const                      = 0;
    virtual void        write_body(BinaryBuffer& buffer) const = 0;

private:
    std::size_t header_size() const;
    void        write_header(BinaryBuffer& buffer) const;

    std::uint16_t m_key;
    std::uint16_t m_version{1};
    std::uint32_t m_correlation_id;
};

class ClientRequest : public ClientRequestOrResponse
{
    using ClientRequestOrResponse::ClientRequestOrResponse;
};

class ClientResponse : public ClientRequestOrResponse
{
    using ClientRequestOrResponse::ClientRequestOrResponse;
};

class ServerCommand : public Deserializable
{
public:
    void deserialize(BinaryBuffer& buffer) final
    {
        read_body(buffer);
    }

protected:
    virtual void read_body(BinaryBuffer& buffer) = 0;
};

class ServerRequestOrResponse : public Deserializable
{
public:
    std::uint32_t get_correlation_id() const
    {
        return m_correlation_id;
    }

    void deserialize(BinaryBuffer& buffer) final
    {
        read_header(buffer);
        read_body(buffer);
    }

protected:
    virtual void read_body(BinaryBuffer& buffer) = 0;

private:
    void read_header(BinaryBuffer& buffer);

    std::uint32_t m_correlation_id;
};

class ServerRequest : public ServerRequestOrResponse
{
public:
    using ServerRequestOrResponse::ServerRequestOrResponse;
};

class ServerResponse : public ServerRequestOrResponse
{
public:
    using ServerRequestOrResponse::ServerRequestOrResponse;
};

class DeclarePublisherRequest : public ClientRequest
{
public:
    DeclarePublisherRequest(std::uint32_t correlation_id, std::uint8_t publisher_id, std::string_view publisher_name, std::string_view stream_name)
     : ClientRequest(CommandKey::DeclarePublisher, correlation_id),
       m_publisher_id{publisher_id},
       m_publisher_name{publisher_name},
       m_stream_name{stream_name}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint8_t     m_publisher_id;
    std::string_view m_publisher_name;
    std::string_view m_stream_name;
};

class PublishCommand : public ClientCommand
{
public:
    static const std::size_t BASE_SERIALIZED_SIZE;

    struct Message {
        std::uint64_t             m_publishing_id;
        boost::asio::const_buffer m_body;

        std::size_t serialized_size() const;
    };

    PublishCommand(std::uint8_t publisher_id, const std::vector<Message>& messages)
     : ClientCommand(CommandKey::Publish),
       m_publisher_id{publisher_id},
       m_messages{messages}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint8_t                m_publisher_id;
    const std::vector<Message>& m_messages;
};

class PublishConfirmCommand : public ServerCommand
{
public:
    std::uint8_t get_publisher_id() const
    {
        return m_publisher_id;
    }

    const std::vector<std::uint64_t>& get_sequences() const
    {
        return m_sequences;
    }

    std::vector<std::uint64_t>& get_sequences()
    {
        return m_sequences;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint8_t               m_publisher_id;
    std::vector<std::uint64_t> m_sequences;
};

class PublishErrorCommand : public ServerCommand
{
public:
    struct PublishingError {
        std::uint64_t m_sequence;
        std::uint16_t m_code;
    };

    std::uint8_t get_publisher_id() const
    {
        return m_publisher_id;
    }

    const std::vector<PublishingError>& get_publishing_errors() const
    {
        return m_publishing_errors;
    }

    std::vector<PublishingError>& get_publishing_errors()
    {
        return m_publishing_errors;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint8_t                 m_publisher_id;
    std::vector<PublishingError> m_publishing_errors;
};

class QueryPublisherSequenceRequest : public ClientRequest
{
public:
    QueryPublisherSequenceRequest(std::uint32_t correlation_id, std::string_view publisher_name, std::string_view stream_name)
     : ClientRequest(CommandKey::QueryPublisherSequence, correlation_id),
       m_publisher_name{publisher_name},
       m_stream_name{stream_name}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::string_view m_publisher_name;
    std::string_view m_stream_name;
};

class QueryPublisherSequenceResponse : public ServerResponse
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }

    std::uint64_t get_sequence() const
    {
        return m_sequence;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t m_response_code;
    std::uint64_t m_sequence;
};

class DeletePublisherRequest : public ClientRequest
{
public:
    DeletePublisherRequest(std::uint32_t correlation_id, std::uint8_t publisher_id)
     : ClientRequest(CommandKey::DeletePublisher, correlation_id),
       m_publisher_id{publisher_id}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint8_t m_publisher_id;
};

class SubscribeRequest : public ClientRequest
{
public:
    SubscribeRequest(std::uint32_t              correlation_id,
                     std::uint8_t               subscription_id,
                     std::string_view           stream_name,
                     const OffsetSpecification& offset,
                     std::uint16_t              credit,
                     const Properties&          properties)
     : ClientRequest(CommandKey::Subscribe, correlation_id),
       m_subscription_id{subscription_id},
       m_stream_name{stream_name},
       m_offset{offset},
       m_credit{credit},
       m_properties{properties}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint8_t               m_subscription_id;
    std::string_view           m_stream_name;
    const OffsetSpecification& m_offset;
    std::uint16_t              m_credit;
    const Properties&          m_properties;
};

class DeliverCommand : public ServerCommand
{
public:
    static const std::uint8_t CHUNK_TYPE_USER              = 0;
    static const std::uint8_t CHUNK_TYPE_TRACKING_DELTA    = 1;
    static const std::uint8_t CHUNK_TYPE_TRACKING_SNAPSHOT = 2;

    struct Chunk {
        //  Osiris Chunk
        std::uint8_t  m_magic_version;
        std::uint8_t  m_chunk_type;
        std::uint16_t m_nb_entries;
        std::uint32_t m_nb_records;
        std::int64_t  m_timestamp;
        std::uint64_t m_epoch;
        std::uint64_t m_offset;
        std::uint32_t m_chunk_crc;
        std::uint32_t m_data_len;
        std::uint32_t m_trailer_len;
        std::uint32_t m_reserved;
    };

    std::uint8_t get_subscription_id() const
    {
        return m_subscription_id;
    }

    const Chunk& get_chunk() const
    {
        return m_chunk;
    }

    Chunk& get_chunk()
    {
        return m_chunk;
    }

    const std::vector<boost::asio::const_buffer>& get_messages() const
    {
        return m_messages;
    }

    std::vector<boost::asio::const_buffer>& get_messages()
    {
        return m_messages;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

protected:
    std::uint8_t                           m_subscription_id;
    Chunk                                  m_chunk;
    BinaryBuffer                           m_data;
    std::vector<boost::asio::const_buffer> m_messages;
};

class CreditCommand : public ClientCommand
{
public:
    CreditCommand(std::uint8_t subscription_id, std::uint16_t credit) : ClientCommand(CommandKey::Credit), m_subscription_id{subscription_id}, m_credit{credit}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint8_t  m_subscription_id;
    std::uint16_t m_credit;
};

class CreditErrorCommand : public ServerCommand
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }

    std::uint8_t get_subscription_id() const
    {
        return m_subscription_id;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t m_response_code;
    std::uint8_t  m_subscription_id;
};

class StoreOffsetCommand : public ClientCommand
{
public:
    StoreOffsetCommand(std::string_view reference, std::string_view stream_name, std::uint64_t offset)
     : ClientCommand(CommandKey::StoreOffset),
       m_reference{reference},
       m_stream_name{stream_name},
       m_offset{offset}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::string_view m_reference;
    std::string_view m_stream_name;
    std::uint64_t    m_offset;
};

class QueryOffsetRequest : public ClientRequest
{
public:
    QueryOffsetRequest(std::uint32_t correlation_id, std::string_view reference, std::string_view stream_name)
     : ClientRequest(CommandKey::QueryOffset, correlation_id),
       m_reference{reference},
       m_stream_name{stream_name}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::string m_reference;
    std::string m_stream_name;
};

class QueryOffsetResponse : public ServerResponse
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }
    std::uint64_t get_offset() const
    {
        return m_offset;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t m_response_code;
    std::uint64_t m_offset;
};

class UnsubscribeRequest : public ClientRequest
{
public:
    UnsubscribeRequest(std::uint32_t correlation_id, std::uint8_t subscription_id)
     : ClientRequest(CommandKey::Unsubscribe, correlation_id),
       m_subscription_id{subscription_id}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint8_t m_subscription_id;
};

class CreateRequest : public ClientRequest
{
public:
    CreateRequest(std::uint32_t correlation_id, std::string_view stream_name, const Properties& arguments)
     : ClientRequest(CommandKey::Create, correlation_id),
       m_stream_name{stream_name},
       m_arguments{arguments}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::string_view  m_stream_name;
    const Properties& m_arguments;
};

class DeleteRequest : public ClientRequest
{
public:
    DeleteRequest(std::uint32_t correlation_id, std::string_view stream_name) : ClientRequest(CommandKey::Delete, correlation_id), m_stream_name{stream_name}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::string_view m_stream_name;
};

class MetadataRequest : public ClientRequest
{
public:
    MetadataRequest(std::uint32_t correlation_id, const std::vector<std::string>& stream_names)
     : ClientRequest(CommandKey::Metadata, correlation_id),
       m_stream_names{stream_names}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    const std::vector<std::string>& m_stream_names;
};

class MetadataResponse : public ServerResponse
{
public:
    struct Broker {
        std::uint16_t m_reference;
        std::string   m_host;
        std::uint32_t m_port;
    };

    struct StreamMetadata {
        std::string                m_stream_name;
        std::uint16_t              m_code;
        std::uint16_t              m_leader;
        std::vector<std::uint16_t> m_replicas;
    };

    const std::vector<Broker>& get_brokers() const
    {
        return m_brokers;
    }

    std::vector<Broker>& get_brokers()
    {
        return m_brokers;
    }

    const std::vector<StreamMetadata>& get_stream_metadata() const
    {
        return m_stream_metadata;
    }

    std::vector<StreamMetadata>& get_stream_metadata()
    {
        return m_stream_metadata;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::vector<Broker>         m_brokers;
    std::vector<StreamMetadata> m_stream_metadata;
};

class MetadataUpdateCommand : public ServerCommand
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }

    std::string_view get_stream_name() const
    {
        return m_stream_name;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t m_response_code;
    std::string   m_stream_name;
};

class PeerPropertiesRequest : public ClientRequest
{
public:
    PeerPropertiesRequest(std::uint32_t correlation_id, const Properties& properties)
     : ClientRequest(CommandKey::PeerProperties, correlation_id),
       m_properties{properties}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    const Properties& m_properties;
};

class PeerPropertiesResponse : public ServerResponse
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }

    const Properties& get_properties() const
    {
        return m_properties;
    }

    Properties& get_properties()
    {
        return m_properties;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t m_response_code;
    Properties    m_properties;
};

class SaslHandshakeRequest : public ClientRequest
{
public:
    SaslHandshakeRequest(std::uint32_t correlation_id) : ClientRequest(CommandKey::SaslHandshake, correlation_id)
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;
};

class SaslHandshakeResponse : public ServerResponse
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }

    const std::vector<std::string>& get_mechanisms() const
    {
        return m_mechanisms;
    }

    std::vector<std::string>& get_mechanisms()
    {
        return m_mechanisms;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t            m_response_code;
    std::vector<std::string> m_mechanisms;
};

class SaslAuthenticateRequest : public ClientRequest
{
public:
    SaslAuthenticateRequest(std::uint32_t correlation_id, std::string_view mechanism, const std::vector<std::uint8_t>& sasl_opaque_data)
     : ClientRequest(CommandKey::SaslAuthenticate, correlation_id),
       m_mechanism{mechanism},
       m_sasl_opaque_data{sasl_opaque_data}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::string_view                 m_mechanism;
    const std::vector<std::uint8_t>& m_sasl_opaque_data;
};

class SaslAuthenticateResponse : public ServerResponse
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }

    const std::vector<std::uint8_t>& get_sasl_opaque_data() const
    {
        return m_sasl_opaque_data;
    }

    std::vector<std::uint8_t>& get_sasl_opaque_data()
    {
        return m_sasl_opaque_data;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t             m_response_code;
    std::vector<std::uint8_t> m_sasl_opaque_data;
};

class ClientTuneCommand : public ClientCommand
{
public:
    ClientTuneCommand(std::uint32_t frame_size, std::uint32_t heartbeat)
     : ClientCommand(0x8000 | static_cast<std::uint16_t>(CommandKey::Tune)),
       m_frame_size{frame_size},
       m_heartbeat{heartbeat}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint32_t m_frame_size;
    std::uint32_t m_heartbeat;
};

class ServerTuneCommand : public ServerCommand
{
public:
    std::uint32_t get_frame_size() const
    {
        return m_frame_size;
    }

    std::uint32_t get_heartbeat() const
    {
        return m_heartbeat;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint32_t m_frame_size;
    std::uint32_t m_heartbeat;
};

class OpenRequest : public ClientRequest
{
public:
    OpenRequest(std::uint32_t correlation_id, std::string_view virtual_host) : ClientRequest(CommandKey::Open, correlation_id), m_virtual_host{virtual_host}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::string m_virtual_host;
};

class OpenResponse : public ServerResponse
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }

    const Properties& get_connection_properties() const
    {
        return m_connection_properties;
    }

    Properties& get_connection_properties()
    {
        return m_connection_properties;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t m_response_code;
    Properties    m_connection_properties;
};

class ClientCloseRequest : public ClientRequest
{
public:
    ClientCloseRequest(std::uint32_t correlation_id, ResponseCode code, std::string_view reason)
     : ClientRequest(CommandKey::Close, correlation_id),
       m_code{static_cast<std::uint16_t>(code)},
       m_reason{reason}
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint16_t    m_code;
    std::string_view m_reason;
};

class ServerCloseRequest : public ServerRequest
{
public:
    std::uint16_t get_closing_code() const
    {
        return m_closing_code;
    }

    std::string_view get_closing_reason() const
    {
        return m_closing_reason;
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t m_closing_code;
    std::string   m_closing_reason;
};

class ClientCloseResponse : public ClientResponse
{
public:
    ClientCloseResponse(std::uint32_t correlation_id, ResponseCode response_code)
     : ClientResponse(0x8000 | static_cast<std::uint16_t>(CommandKey::Close), correlation_id),
       m_response_code(static_cast<std::uint16_t>(response_code))
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;

private:
    std::uint16_t m_response_code;
};

class ClientHeartbeatCommand : public ClientCommand
{
public:
    ClientHeartbeatCommand() : ClientCommand(CommandKey::Heartbeat)
    {
    }

protected:
    std::size_t body_size() const override;
    void        write_body(BinaryBuffer& buffer) const override;
};

class GenericResponse : public ServerResponse
{
public:
    ResponseCode get_response_code() const
    {
        return static_cast<ResponseCode>(m_response_code);
    }

protected:
    void read_body(BinaryBuffer& buffer) override;

private:
    std::uint16_t m_response_code;
};

}  // namespace hareflow::detail
