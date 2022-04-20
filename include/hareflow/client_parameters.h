#pragma once

#include <cstdint>

#include <chrono>
#include <functional>
#include <string>

#include "hareflow/qpid_proton_codec.h"
#include "hareflow/types.h"

namespace hareflow {

using ChunkListener =
    std::function<void(Client& client, std::uint8_t subscription_id, std::int64_t timestamp, std::uint64_t offset, std::uint32_t message_count)>;
using MessageListener        = std::function<void(std::uint8_t subscription_id, std::int64_t chunk_timestamp, std::uint64_t offset, MessagePtr message)>;
using PublishConfirmListener = std::function<void(std::uint8_t publisher_id, std::uint64_t publishing_id)>;
using PublishErrorListener   = std::function<void(std::uint8_t publisher_id, std::uint64_t publishing_id, ResponseCode error_code)>;
using MetadataListener       = std::function<void(std::string_view stream, ResponseCode error_code)>;
using CreditErrorListener    = std::function<void(std::uint8_t subscription_id, ResponseCode error_code)>;
using ShutdownListener       = std::function<void(ShutdownReason reason)>;

class HAREFLOW_EXPORT ClientParameters
{
public:
    const std::string&            get_host() const;
    std::uint16_t                 get_port() const;
    bool                          get_use_ssl() const;
    bool                          get_verify_host() const;
    const std::string&            get_user() const;
    const std::string&            get_password() const;
    const std::string&            get_virtual_host() const;
    std::chrono::seconds          get_rpc_timeout() const;
    std::chrono::seconds          get_requested_heartbeat() const;
    std::uint32_t                 get_requested_max_frame_size() const;
    CodecPtr                      get_codec() const;
    const Properties&             get_client_properties() const;
    const ChunkListener&          get_chunk_listener() const;
    const MessageListener&        get_message_listener() const;
    const PublishConfirmListener& get_publish_confirm_listener() const;
    const PublishErrorListener&   get_publish_error_listener() const;
    const MetadataListener&       get_metadata_listener() const;
    const CreditErrorListener&    get_credit_error_listener() const;
    const ShutdownListener&       get_shutdown_listener() const;

    std::string&            get_host();
    std::string&            get_user();
    std::string&            get_password();
    std::string&            get_virtual_host();
    Properties&             get_client_properties();
    ChunkListener&          get_chunk_listener();
    MessageListener&        get_message_listener();
    PublishConfirmListener& get_publish_confirm_listener();
    PublishErrorListener&   get_publish_error_listener();
    MetadataListener&       get_metadata_listener();
    CreditErrorListener&    get_credit_error_listener();
    ShutdownListener&       get_shutdown_listener();

    ClientParameters& with_host(std::string host) &;
    ClientParameters& with_port(std::uint16_t port) &;
    ClientParameters& with_use_ssl(bool use_ssl) &;
    ClientParameters& with_verify_host(bool verify_host) &;
    ClientParameters& with_user(std::string user) &;
    ClientParameters& with_password(std::string password) &;
    ClientParameters& with_virtual_host(std::string virtual_host) &;
    ClientParameters& with_requested_heartbeat(std::chrono::seconds requested_heartbeat) &;
    ClientParameters& with_requested_max_frame_size(std::uint32_t requested_max_frame_size) &;
    ClientParameters& with_rpc_timeout(std::chrono::seconds rpc_timeout) &;
    ClientParameters& with_codec(CodecPtr codec) &;
    ClientParameters& with_client_properties(Properties client_properties) &;
    ClientParameters& with_chunk_listener(ChunkListener chunk_listener) &;
    ClientParameters& with_message_listener(MessageListener message_listener) &;
    ClientParameters& with_publish_confirm_listener(PublishConfirmListener publish_confirm_listener) &;
    ClientParameters& with_publish_error_listener(PublishErrorListener publish_error_listener) &;
    ClientParameters& with_metadata_listener(MetadataListener metadata_listener) &;
    ClientParameters& with_credit_error_listener(CreditErrorListener credit_error_listener) &;
    ClientParameters& with_shutdown_listener(ShutdownListener shutdown_listener) &;

    ClientParameters with_host(std::string host) &&;
    ClientParameters with_port(std::uint16_t port) &&;
    ClientParameters with_use_ssl(bool use_ssl) &&;
    ClientParameters with_verify_host(bool verify_host) &&;
    ClientParameters with_user(std::string user) &&;
    ClientParameters with_password(std::string password) &&;
    ClientParameters with_virtual_host(std::string virtual_host) &&;
    ClientParameters with_requested_heartbeat(std::chrono::seconds requested_heartbeat) &&;
    ClientParameters with_requested_max_frame_size(std::uint32_t requested_max_frame_size) &&;
    ClientParameters with_rpc_timeout(std::chrono::seconds rpc_timeout) &&;
    ClientParameters with_codec(CodecPtr codec) &&;
    ClientParameters with_client_properties(Properties client_properties) &&;
    ClientParameters with_chunk_listener(ChunkListener chunk_listener) &&;
    ClientParameters with_message_listener(MessageListener message_listener) &&;
    ClientParameters with_publish_confirm_listener(PublishConfirmListener publish_confirm_listener) &&;
    ClientParameters with_publish_error_listener(PublishErrorListener publish_error_listener) &&;
    ClientParameters with_metadata_listener(MetadataListener metadata_listener) &&;
    ClientParameters with_credit_error_listener(CreditErrorListener credit_error_listener) &&;
    ClientParameters with_shutdown_listener(ShutdownListener shutdown_listener) &&;

private:
    std::string            m_host                     = "localhost";
    std::uint16_t          m_port                     = 0;
    bool                   m_use_ssl                  = false;
    bool                   m_verify_host              = true;
    std::string            m_user                     = "guest";
    std::string            m_password                 = "guest";
    std::string            m_virtual_host             = "/";
    std::chrono::seconds   m_requested_heartbeat      = std::chrono::seconds(60);
    std::uint32_t          m_requested_max_frame_size = 1024 * 1024;
    std::chrono::seconds   m_rpc_timeout              = std::chrono::seconds(10);
    CodecPtr               m_codec                    = std::make_shared<QpidProtonCodec>();
    Properties             m_client_properties        = {};
    ChunkListener          m_chunk_listener           = {};
    MessageListener        m_message_listener         = {};
    PublishConfirmListener m_publish_confirm_listener = {};
    PublishErrorListener   m_publish_error_listener   = {};
    MetadataListener       m_metadata_listener        = {};
    CreditErrorListener    m_credit_error_listener    = {};
    ShutdownListener       m_shutdown_listener        = {};
};

}  // namespace hareflow