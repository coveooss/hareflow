#include "hareflow/detail/commands.h"

namespace hareflow::detail {

const std::size_t PublishCommand::BASE_SERIALIZED_SIZE = PublishCommand(0, {}).serialized_size();

std::size_t ClientCommand::header_size() const
{
    return sizeof(m_key) + sizeof(m_version);
}

void ClientCommand::write_header(BinaryBuffer& buffer) const
{
    buffer.write_ushort(m_key);
    buffer.write_ushort(m_version);
}

std::size_t ClientRequestOrResponse::header_size() const
{
    return sizeof(m_key) + sizeof(m_version) + sizeof(m_correlation_id);
}

void ClientRequestOrResponse::write_header(BinaryBuffer& buffer) const
{
    buffer.write_ushort(m_key);
    buffer.write_ushort(m_version);
    buffer.write_uint(m_correlation_id);
}

void ServerRequestOrResponse::read_header(BinaryBuffer& buffer)
{
    m_correlation_id = buffer.read_uint();
}

std::size_t DeclarePublisherRequest::body_size() const
{
    return sizeof(m_publisher_id) + BinaryBuffer::serialized_size(m_publisher_name) + BinaryBuffer::serialized_size(m_stream_name);
}

void DeclarePublisherRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_byte(m_publisher_id);
    buffer.write_string(m_publisher_name);
    buffer.write_string(m_stream_name);
}

std::size_t PublishCommand::body_size() const
{
    std::size_t size = sizeof(m_publisher_id);
    size += BinaryBuffer::serialized_size(m_messages, [](const auto& elem) { return elem.serialized_size(); });
    return size;
}

std::size_t PublishCommand::Message::serialized_size() const
{
    return sizeof(m_publishing_id) + BinaryBuffer::serialized_size(boost::asio::buffer(m_body));
}

void PublishCommand::write_body(BinaryBuffer& buffer) const
{
    buffer.write_byte(m_publisher_id);
    buffer.write_array(m_messages, [](auto& buffer, const auto& elem) {
        buffer.write_ulong(elem.m_publishing_id);
        buffer.write_blob(boost::asio::buffer(elem.m_body));
    });
}

void PublishConfirmCommand::read_body(BinaryBuffer& buffer)
{
    m_publisher_id = buffer.read_byte();
    buffer.read_array(m_sequences, [](auto& buffer) { return buffer.read_ulong(); });
}

void PublishErrorCommand::read_body(BinaryBuffer& buffer)
{
    m_publisher_id = buffer.read_byte();
    buffer.read_array(m_publishing_errors, [](auto& buffer) {
        PublishingError publishing_error;
        publishing_error.m_sequence = buffer.read_ulong();
        publishing_error.m_code     = buffer.read_ushort();
        return publishing_error;
    });
}

std::size_t QueryPublisherSequenceRequest::body_size() const
{
    return BinaryBuffer::serialized_size(m_publisher_name) + BinaryBuffer::serialized_size(m_stream_name);
}

void QueryPublisherSequenceRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_string(m_publisher_name);
    buffer.write_string(m_stream_name);
}

void QueryPublisherSequenceResponse::read_body(BinaryBuffer& buffer)
{
    m_response_code = buffer.read_ushort();
    m_sequence      = buffer.read_ulong();
}

std::size_t DeletePublisherRequest::body_size() const
{
    return sizeof(m_publisher_id);
}

void DeletePublisherRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_byte(m_publisher_id);
}

std::size_t SubscribeRequest::body_size() const
{
    std::size_t size = sizeof(m_subscription_id) + BinaryBuffer::serialized_size(m_stream_name) + sizeof(m_offset.get_type());
    if (m_offset.is_offset() || m_offset.is_timestamp()) {
        size += sizeof(m_offset.get_offset());
    }
    size += sizeof(m_credit);
    size += BinaryBuffer::serialized_size(m_properties, [](const auto& elem) {
        return BinaryBuffer::serialized_size(elem.first) + BinaryBuffer::serialized_size(elem.second);
    });
    return size;
}

void SubscribeRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_byte(m_subscription_id);
    buffer.write_string(m_stream_name);
    buffer.write_ushort(m_offset.get_type());
    if (m_offset.is_offset() || m_offset.is_timestamp()) {
        buffer.write_ulong(m_offset.get_offset());
    }
    buffer.write_ushort(m_credit);
    buffer.write_array(m_properties, [](auto& buffer, const auto& elem) {
        buffer.write_string(elem.first);
        buffer.write_string(elem.second);
    });
}

void DeliverCommand::read_body(BinaryBuffer& buffer)
{
    m_subscription_id       = buffer.read_byte();
    m_chunk.m_magic_version = buffer.read_byte();
    m_chunk.m_chunk_type    = buffer.read_byte();
    m_chunk.m_nb_entries    = buffer.read_ushort();
    m_chunk.m_nb_records    = buffer.read_uint();
    m_chunk.m_timestamp     = buffer.read_long();
    m_chunk.m_epoch         = buffer.read_ulong();
    m_chunk.m_offset        = buffer.read_ulong();
    m_chunk.m_chunk_crc     = buffer.read_uint();
    m_chunk.m_data_len      = buffer.read_uint();
    m_chunk.m_trailer_len   = buffer.read_uint();
    m_chunk.m_reserved      = buffer.read_uint();

    m_messages.reserve(m_chunk.m_nb_entries);
    m_data.write_bytes(buffer.read_bytes(m_chunk.m_data_len));
    for (std::uint16_t i = 0; i < m_chunk.m_nb_entries; ++i) {
        if ((m_data.read_byte() & 0x80) != 0) {
            throw StreamException("Sub entry batching is not currently supported");
        }
        m_data.read_cursor(m_data.read_cursor() - 1);
        m_messages.emplace_back(m_data.read_blob());
    }
}

std::size_t CreditCommand::body_size() const
{
    return sizeof(m_subscription_id) + sizeof(m_credit);
}

void CreditCommand::write_body(BinaryBuffer& buffer) const
{
    buffer.write_byte(m_subscription_id);
    buffer.write_ushort(m_credit);
}

void CreditErrorCommand::read_body(BinaryBuffer& buffer)
{
    m_response_code   = buffer.read_ushort();
    m_subscription_id = buffer.read_byte();
}

std::size_t StoreOffsetCommand::body_size() const
{
    return BinaryBuffer::serialized_size(m_reference) + BinaryBuffer::serialized_size(m_stream_name) + sizeof(m_offset);
}

void StoreOffsetCommand::write_body(BinaryBuffer& buffer) const
{
    buffer.write_string(m_reference);
    buffer.write_string(m_stream_name);
    buffer.write_ulong(m_offset);
}

std::size_t QueryOffsetRequest::body_size() const
{
    return BinaryBuffer::serialized_size(m_reference) + BinaryBuffer::serialized_size(m_stream_name);
}

void QueryOffsetRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_string(m_reference);
    buffer.write_string(m_stream_name);
}

void QueryOffsetResponse::read_body(BinaryBuffer& buffer)
{
    m_response_code = buffer.read_ushort();
    m_offset        = buffer.read_ulong();
}

std::size_t UnsubscribeRequest::body_size() const
{
    return sizeof(m_subscription_id);
}

void UnsubscribeRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_byte(m_subscription_id);
}

std::size_t CreateRequest::body_size() const
{
    std::size_t size = BinaryBuffer::serialized_size(m_stream_name);
    size += BinaryBuffer::serialized_size(m_arguments, [](const auto& elem) {
        return BinaryBuffer::serialized_size(elem.first) + BinaryBuffer::serialized_size(elem.second);
    });
    return size;
}

void CreateRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_string(m_stream_name);
    buffer.write_array(m_arguments, [](auto& buffer, const auto& elem) {
        buffer.write_string(elem.first);
        buffer.write_string(elem.second);
    });
}

std::size_t DeleteRequest::body_size() const
{
    return BinaryBuffer::serialized_size(m_stream_name);
}

void DeleteRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_string(m_stream_name);
}

std::size_t MetadataRequest::body_size() const
{
    return BinaryBuffer::serialized_size(m_stream_names, [](const auto& elem) { return BinaryBuffer::serialized_size(elem); });
}

void MetadataRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_array(m_stream_names, [](auto& buffer, const auto& elem) { buffer.write_string(elem); });
}

void MetadataResponse::read_body(BinaryBuffer& buffer)
{
    buffer.read_array(m_brokers, [](auto& buffer) {
        Broker broker;
        broker.m_reference = buffer.read_ushort();
        broker.m_host      = buffer.read_string();
        broker.m_port      = buffer.read_uint();
        return broker;
    });
    buffer.read_array(m_stream_metadata, [](auto& buffer) {
        StreamMetadata stream;
        stream.m_stream_name = buffer.read_string();
        stream.m_code        = buffer.read_ushort();
        stream.m_leader      = buffer.read_ushort();
        buffer.read_array(stream.m_replicas, [](auto& buffer) { return buffer.read_ushort(); });
        return stream;
    });
}

void MetadataUpdateCommand::read_body(BinaryBuffer& buffer)
{
    m_response_code = buffer.read_ushort();
    if (static_cast<ResponseCode>(m_response_code) == ResponseCode::StreamNotAvailable) {
        m_stream_name = buffer.read_string();
    }
}

std::size_t PeerPropertiesRequest::body_size() const
{
    return BinaryBuffer::serialized_size(m_properties, [](const auto& elem) {
        return BinaryBuffer::serialized_size(elem.first) + BinaryBuffer::serialized_size(elem.second);
    });
}

void PeerPropertiesRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_array(m_properties, [](auto& buffer, const auto& elem) {
        buffer.write_string(elem.first);
        buffer.write_string(elem.second);
    });
}

void PeerPropertiesResponse::read_body(BinaryBuffer& buffer)
{
    m_response_code = buffer.read_ushort();
    buffer.read_array(m_properties, [](auto& buffer) { return std::make_pair(std::string(buffer.read_string()), std::string(buffer.read_string())); });
}

std::size_t SaslHandshakeRequest::body_size() const
{
    return 0;
}

void SaslHandshakeRequest::write_body(BinaryBuffer& /*buffer*/) const
{
    // Header only
}

void SaslHandshakeResponse::read_body(BinaryBuffer& buffer)
{
    m_response_code = buffer.read_ushort();
    buffer.read_array(m_mechanisms, [](auto& buffer) { return std::string(buffer.read_string()); });
}

std::size_t SaslAuthenticateRequest::body_size() const
{
    return BinaryBuffer::serialized_size(m_mechanism) + BinaryBuffer::serialized_size(boost::asio::buffer(m_sasl_opaque_data));
}

void SaslAuthenticateRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_string(m_mechanism);
    buffer.write_blob(boost::asio::buffer(m_sasl_opaque_data));
}

void SaslAuthenticateResponse::read_body(BinaryBuffer& buffer)
{
    m_response_code = buffer.read_ushort();
    if (static_cast<ResponseCode>(m_response_code) == ResponseCode::SaslChallenge) {
        auto blob = buffer.read_blob();
        m_sasl_opaque_data.resize(blob.size());
        memcpy(m_sasl_opaque_data.data(), blob.data(), blob.size());
    }
}

std::size_t ClientTuneCommand::body_size() const
{
    return sizeof(m_frame_size) + sizeof(m_heartbeat);
}

void ClientTuneCommand::write_body(BinaryBuffer& buffer) const
{
    buffer.write_uint(m_frame_size);
    buffer.write_uint(m_heartbeat);
}

void ServerTuneCommand::read_body(BinaryBuffer& buffer)
{
    m_frame_size = buffer.read_uint();
    m_heartbeat  = buffer.read_uint();
}

std::size_t OpenRequest::body_size() const
{
    return BinaryBuffer::serialized_size(m_virtual_host);
}

void OpenRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_string(m_virtual_host);
}

void OpenResponse::read_body(BinaryBuffer& buffer)
{
    m_response_code = buffer.read_ushort();
    buffer.read_array(m_connection_properties,
                      [](auto& buffer) { return std::make_pair(std::string(buffer.read_string()), std::string(buffer.read_string())); });
}

std::size_t ClientCloseRequest::body_size() const
{
    return sizeof(m_code) + BinaryBuffer::serialized_size(m_reason);
}

void ClientCloseRequest::write_body(BinaryBuffer& buffer) const
{
    buffer.write_ushort(m_code);
    buffer.write_string(m_reason);
}

void ServerCloseRequest::read_body(BinaryBuffer& buffer)
{
    m_closing_code   = buffer.read_ushort();
    m_closing_reason = buffer.read_string();
}

std::size_t ClientCloseResponse::body_size() const
{
    return sizeof(m_response_code);
}

void ClientCloseResponse::write_body(BinaryBuffer& buffer) const
{
    buffer.write_ushort(m_response_code);
}

std::size_t ClientHeartbeatCommand::body_size() const
{
    return 0;
}

void ClientHeartbeatCommand::write_body(BinaryBuffer& /*buffer*/) const
{
    // Header only
}

void GenericResponse::read_body(BinaryBuffer& buffer)
{
    m_response_code = buffer.read_ushort();
}

}  // namespace hareflow::detail
