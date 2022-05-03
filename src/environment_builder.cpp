#include "hareflow/environment_builder.h"

#include "hareflow/client_parameters.h"
#include "hareflow/exceptions.h"
#include "hareflow/detail/environment_impl.h"

namespace hareflow {

EnvironmentBuilder& EnvironmentBuilder::host(std::string host) &
{
    m_hosts.clear();
    m_hosts.emplace_back(std::move(host));
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::hosts(std::vector<std::string> hosts) &
{
    if (hosts.empty()) {
        throw InvalidInputException{"hosts must not be empty"};
    }
    m_hosts = std::move(hosts);
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::port(std::uint16_t port) &
{
    m_client_parameters.with_port(port);
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::use_ssl(bool use_ssl) &
{
    m_client_parameters.with_use_ssl(use_ssl);
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::verify_host(bool verify_host) &
{
    m_client_parameters.with_verify_host(verify_host);
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::username(std::string username) &
{
    m_client_parameters.with_user(std::move(username));
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::password(std::string password) &
{
    m_client_parameters.with_password(std::move(password));
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::virtual_host(std::string virtual_host) &
{
    m_client_parameters.with_virtual_host(std::move(virtual_host));
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::requested_heartbeat(std::chrono::seconds requested_heartbeat) &
{
    if (requested_heartbeat < std::chrono::seconds::zero()) {
        throw InvalidInputException{"requested heartbeat must not be negative"};
    }
    m_client_parameters.with_requested_heartbeat(requested_heartbeat);
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::requested_max_frame_size(std::uint32_t requested_max_frame_size) &
{
    m_client_parameters.with_requested_max_frame_size(requested_max_frame_size);
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::rpc_timeout(std::chrono::seconds rpc_timeout) &
{
    if (rpc_timeout <= std::chrono::seconds::zero()) {
        throw InvalidInputException{"rpc timeout must be positive"};
    }
    m_client_parameters.with_rpc_timeout(rpc_timeout);
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::recovery_retry_delay(std::chrono::milliseconds recovery_retry_delay) &
{
    if (recovery_retry_delay <= std::chrono::milliseconds::zero()) {
        throw InvalidInputException{"recovery retry delay must be positive"};
    }
    m_recovery_retry_delay = recovery_retry_delay;
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::client_properties(Properties client_properties) &
{
    m_client_parameters.with_client_properties(std::move(client_properties));
    return *this;
}

EnvironmentBuilder& EnvironmentBuilder::codec(CodecPtr codec) &
{
    if (codec == nullptr) {
        throw InvalidInputException("codec must not be null");
    }
    m_client_parameters.with_codec(std::move(codec));
    return *this;
}

EnvironmentBuilder EnvironmentBuilder::host(std::string host) &&
{
    return std::move(this->host(std::move(host)));
}

EnvironmentBuilder EnvironmentBuilder::hosts(std::vector<std::string> hosts) &&
{
    return std::move(this->hosts(std::move(hosts)));
}

EnvironmentBuilder EnvironmentBuilder::port(std::uint16_t port) &&
{
    return std::move(this->port(port));
}

EnvironmentBuilder EnvironmentBuilder::use_ssl(bool use_ssl) &&
{
    return std::move(this->use_ssl(use_ssl));
}

EnvironmentBuilder EnvironmentBuilder::verify_host(bool verify_host) &&
{
    return std::move(this->verify_host(verify_host));
}

EnvironmentBuilder EnvironmentBuilder::username(std::string username) &&
{
    return std::move(this->username(std::move(username)));
}

EnvironmentBuilder EnvironmentBuilder::password(std::string password) &&
{
    return std::move(this->password(std::move(password)));
}

EnvironmentBuilder EnvironmentBuilder::virtual_host(std::string virtual_host) &&
{
    return std::move(this->virtual_host(std::move(virtual_host)));
}

EnvironmentBuilder EnvironmentBuilder::requested_heartbeat(std::chrono::seconds requested_heartbeat) &&
{
    return std::move(this->requested_heartbeat(requested_heartbeat));
}

EnvironmentBuilder EnvironmentBuilder::requested_max_frame_size(std::uint32_t requested_max_frame_size) &&
{
    return std::move(this->requested_max_frame_size(requested_max_frame_size));
}

EnvironmentBuilder EnvironmentBuilder::rpc_timeout(std::chrono::seconds rpc_timeout) &&
{
    return std::move(this->rpc_timeout(rpc_timeout));
}

EnvironmentBuilder EnvironmentBuilder::recovery_retry_delay(std::chrono::milliseconds recovery_retry_delay) &&
{
    return std::move(this->recovery_retry_delay(recovery_retry_delay));
}

EnvironmentBuilder EnvironmentBuilder::client_properties(Properties client_properties) &&
{
    return std::move(this->client_properties(std::move(client_properties)));
}

EnvironmentBuilder EnvironmentBuilder::codec(CodecPtr codec) &&
{
    return std::move(this->codec(std::move(codec)));
}

EnvironmentPtr EnvironmentBuilder::build() const&
{
    return detail::EnvironmentImpl::create(m_hosts, m_recovery_retry_delay, m_client_parameters);
}

EnvironmentPtr EnvironmentBuilder::build() &&
{
    return detail::EnvironmentImpl::create(std::move(m_hosts), m_recovery_retry_delay, std::move(m_client_parameters));
}

}  // namespace hareflow