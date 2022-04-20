#pragma once

#include <cstdint>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "hareflow/client_parameters.h"
#include "hareflow/environment.h"
#include "hareflow/types.h"

namespace hareflow {

class HAREFLOW_EXPORT EnvironmentBuilder
{
public:
    EnvironmentBuilder& host(std::string host) &;
    EnvironmentBuilder& hosts(std::vector<std::string> hosts) &;
    EnvironmentBuilder& port(std::uint16_t port) &;
    EnvironmentBuilder& use_ssl(bool use_ssl) &;
    EnvironmentBuilder& verify_host(bool verify_host) &;
    EnvironmentBuilder& username(std::string username) &;
    EnvironmentBuilder& password(std::string password) &;
    EnvironmentBuilder& virtual_host(std::string virtual_host) &;
    EnvironmentBuilder& requested_heartbeat(std::chrono::seconds requested_heartbeat) &;
    EnvironmentBuilder& requested_max_frame_size(std::uint32_t requested_max_frame_size) &;
    EnvironmentBuilder& rpc_timeout(std::chrono::seconds rpc_timeout) &;
    EnvironmentBuilder& recovery_retry_delay(std::chrono::seconds recovery_retry_delay) &;
    EnvironmentBuilder& client_properties(Properties client_properties) &;
    EnvironmentBuilder& codec(CodecPtr codec) &;

    EnvironmentBuilder host(std::string host) &&;
    EnvironmentBuilder hosts(std::vector<std::string> hosts) &&;
    EnvironmentBuilder port(std::uint16_t port) &&;
    EnvironmentBuilder use_ssl(bool use_ssl) &&;
    EnvironmentBuilder verify_host(bool verify_host) &&;
    EnvironmentBuilder username(std::string username) &&;
    EnvironmentBuilder password(std::string password) &&;
    EnvironmentBuilder virtual_host(std::string virtual_host) &&;
    EnvironmentBuilder requested_heartbeat(std::chrono::seconds requested_heartbeat) &&;
    EnvironmentBuilder requested_max_frame_size(std::uint32_t requested_max_frame_size) &&;
    EnvironmentBuilder rpc_timeout(std::chrono::seconds rpc_timeout) &&;
    EnvironmentBuilder recovery_retry_delay(std::chrono::seconds recovery_retry_delay) &&;
    EnvironmentBuilder client_properties(Properties client_properties) &&;
    EnvironmentBuilder codec(CodecPtr codec) &&;

    EnvironmentPtr build() const&;
    EnvironmentPtr build() &&;

private:
    std::vector<std::string> m_hosts                = {"localhost"};
    std::chrono::seconds     m_recovery_retry_delay = std::chrono::seconds{5};
    ClientParameters         m_client_parameters    = {};
};

}  // namespace hareflow