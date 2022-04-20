#include "hareflow/client_parameters.h"

#include "hareflow/exceptions.h"

namespace hareflow {

const std::string& ClientParameters::get_host() const
{
    return m_host;
}

std::uint16_t ClientParameters::get_port() const
{
    return m_port;
}

bool ClientParameters::get_use_ssl() const
{
    return m_use_ssl;
}

bool ClientParameters::get_verify_host() const
{
    return m_verify_host;
}

const std::string& ClientParameters::get_user() const
{
    return m_user;
}

const std::string& ClientParameters::get_password() const
{
    return m_password;
}

const std::string& ClientParameters::get_virtual_host() const
{
    return m_virtual_host;
}

std::chrono::seconds ClientParameters::get_rpc_timeout() const
{
    return m_rpc_timeout;
}

std::chrono::seconds ClientParameters::get_requested_heartbeat() const
{
    return m_requested_heartbeat;
}

std::uint32_t ClientParameters::get_requested_max_frame_size() const
{
    return m_requested_max_frame_size;
}

CodecPtr ClientParameters::get_codec() const
{
    return m_codec;
}

const Properties& ClientParameters::get_client_properties() const
{
    return m_client_properties;
}

const ChunkListener& ClientParameters::get_chunk_listener() const
{
    return m_chunk_listener;
}

const MessageListener& ClientParameters::get_message_listener() const
{
    return m_message_listener;
}

const PublishConfirmListener& ClientParameters::get_publish_confirm_listener() const
{
    return m_publish_confirm_listener;
}

const PublishErrorListener& ClientParameters::get_publish_error_listener() const
{
    return m_publish_error_listener;
}

const MetadataListener& ClientParameters::get_metadata_listener() const
{
    return m_metadata_listener;
}

const CreditErrorListener& ClientParameters::get_credit_error_listener() const
{
    return m_credit_error_listener;
}

const ShutdownListener& ClientParameters::get_shutdown_listener() const
{
    return m_shutdown_listener;
}

std::string& ClientParameters::get_host()
{
    return m_host;
}

std::string& ClientParameters::get_user()
{
    return m_user;
}

std::string& ClientParameters::get_password()
{
    return m_password;
}

std::string& ClientParameters::get_virtual_host()
{
    return m_virtual_host;
}

Properties& ClientParameters::get_client_properties()
{
    return m_client_properties;
}

ChunkListener& ClientParameters::get_chunk_listener()
{
    return m_chunk_listener;
}

MessageListener& ClientParameters::get_message_listener()
{
    return m_message_listener;
}

PublishConfirmListener& ClientParameters::get_publish_confirm_listener()
{
    return m_publish_confirm_listener;
}

PublishErrorListener& ClientParameters::get_publish_error_listener()
{
    return m_publish_error_listener;
}

MetadataListener& ClientParameters::get_metadata_listener()
{
    return m_metadata_listener;
}

CreditErrorListener& ClientParameters::get_credit_error_listener()
{
    return m_credit_error_listener;
}

ShutdownListener& ClientParameters::get_shutdown_listener()
{
    return m_shutdown_listener;
}

ClientParameters& ClientParameters::with_host(std::string host) &
{
    m_host = std::move(host);
    return *this;
}

ClientParameters& ClientParameters::with_port(std::uint16_t port) &
{
    m_port = port;
    return *this;
}

ClientParameters& ClientParameters::with_use_ssl(bool use_ssl) &
{
    m_use_ssl = use_ssl;
    return *this;
}

ClientParameters& ClientParameters::with_verify_host(bool verify_host) &
{
    m_verify_host = verify_host;
    return *this;
}

ClientParameters& ClientParameters::with_user(std::string user) &
{
    m_user = std::move(user);
    return *this;
}

ClientParameters& ClientParameters::with_password(std::string password) &
{
    m_password = std::move(password);
    return *this;
}

ClientParameters& ClientParameters::with_virtual_host(std::string virtual_host) &
{
    m_virtual_host = std::move(virtual_host);
    return *this;
}

ClientParameters& ClientParameters::with_rpc_timeout(std::chrono::seconds rpc_timeout) &
{
    m_rpc_timeout = rpc_timeout;
    return *this;
}

ClientParameters& ClientParameters::with_requested_heartbeat(std::chrono::seconds requested_heartbeat) &
{
    m_requested_heartbeat = requested_heartbeat;
    return *this;
}

ClientParameters& ClientParameters::with_requested_max_frame_size(std::uint32_t requested_max_frame_size) &
{
    m_requested_max_frame_size = requested_max_frame_size;
    return *this;
}

ClientParameters& ClientParameters::with_codec(CodecPtr codec) &
{
    m_codec = std::move(codec);
    return *this;
}

ClientParameters& ClientParameters::with_client_properties(Properties client_properties) &
{
    m_client_properties = std::move(client_properties);
    return *this;
}

ClientParameters& ClientParameters::with_chunk_listener(ChunkListener chunk_listener) &
{
    m_chunk_listener = std::move(chunk_listener);
    return *this;
}

ClientParameters& ClientParameters::with_message_listener(MessageListener message_listener) &
{
    m_message_listener = std::move(message_listener);
    return *this;
}

ClientParameters& ClientParameters::with_publish_confirm_listener(PublishConfirmListener publish_confirm_listener) &
{
    m_publish_confirm_listener = std::move(publish_confirm_listener);
    return *this;
}

ClientParameters& ClientParameters::with_publish_error_listener(PublishErrorListener publish_error_listener) &
{
    m_publish_error_listener = std::move(publish_error_listener);
    return *this;
}

ClientParameters& ClientParameters::with_metadata_listener(MetadataListener metadata_listener) &
{
    m_metadata_listener = std::move(metadata_listener);
    return *this;
}

ClientParameters& ClientParameters::with_credit_error_listener(CreditErrorListener credit_error_listener) &
{
    m_credit_error_listener = std::move(credit_error_listener);
    return *this;
}

ClientParameters& ClientParameters::with_shutdown_listener(ShutdownListener shutdown_listener) &
{
    m_shutdown_listener = std::move(shutdown_listener);
    return *this;
}

ClientParameters ClientParameters::with_host(std::string host) &&
{
    return std::move(with_host(std::move(host)));
}

ClientParameters ClientParameters::with_port(std::uint16_t port) &&
{
    return std::move(with_port(port));
}

ClientParameters ClientParameters::with_use_ssl(bool use_ssl) &&
{
    return std::move(with_use_ssl(use_ssl));
}

ClientParameters ClientParameters::with_verify_host(bool verify_host) &&
{
    return std::move(with_verify_host(verify_host));
}

ClientParameters ClientParameters::with_user(std::string user) &&
{
    return std::move(with_user(std::move(user)));
}

ClientParameters ClientParameters::with_password(std::string password) &&
{
    return std::move(with_password(std::move(password)));
}

ClientParameters ClientParameters::with_virtual_host(std::string virtual_host) &&
{
    return std::move(with_virtual_host(std::move(virtual_host)));
}

ClientParameters ClientParameters::with_rpc_timeout(std::chrono::seconds rpc_timeout) &&
{
    return std::move(with_rpc_timeout(rpc_timeout));
}

ClientParameters ClientParameters::with_requested_heartbeat(std::chrono::seconds requested_heartbeat) &&
{
    return std::move(with_requested_heartbeat(requested_heartbeat));
}

ClientParameters ClientParameters::with_requested_max_frame_size(std::uint32_t requested_max_frame_size) &&
{
    return std::move(with_requested_max_frame_size(requested_max_frame_size));
}

ClientParameters ClientParameters::with_codec(CodecPtr codec) &&
{
    return std::move(with_codec(std::move(codec)));
}

ClientParameters ClientParameters::with_client_properties(Properties client_properties) &&
{
    return std::move(with_client_properties(std::move(client_properties)));
}

ClientParameters ClientParameters::with_chunk_listener(ChunkListener chunk_listener) &&
{
    return std::move(with_chunk_listener(std::move(chunk_listener)));
}

ClientParameters ClientParameters::with_message_listener(MessageListener message_listener) &&
{
    return std::move(with_message_listener(std::move(message_listener)));
}

ClientParameters ClientParameters::with_publish_confirm_listener(PublishConfirmListener publish_confirm_listener) &&
{
    return std::move(with_publish_confirm_listener(std::move(publish_confirm_listener)));
}

ClientParameters ClientParameters::with_publish_error_listener(PublishErrorListener publish_error_listener) &&
{
    return std::move(with_publish_error_listener(std::move(publish_error_listener)));
}

ClientParameters ClientParameters::with_metadata_listener(MetadataListener metadata_listener) &&
{
    return std::move(with_metadata_listener(std::move(metadata_listener)));
}

ClientParameters ClientParameters::with_credit_error_listener(CreditErrorListener credit_error_listener) &&
{
    return std::move(with_credit_error_listener(std::move(credit_error_listener)));
}

ClientParameters ClientParameters::with_shutdown_listener(ShutdownListener shutdown_listener) &&
{
    return std::move(with_shutdown_listener(std::move(shutdown_listener)));
}

}  // namespace hareflow