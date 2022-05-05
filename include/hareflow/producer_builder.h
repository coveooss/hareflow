#pragma once

#include <cstdint>

#include <chrono>
#include <memory>
#include <string>

#include "hareflow/types.h"
#include "hareflow/producer.h"

namespace hareflow {

class HAREFLOW_EXPORT ProducerBuilder
{
public:
    ProducerBuilder(detail::InternalEnvironmentPtr environment);
    ProducerBuilder(const ProducerBuilder&) = default;
    ProducerBuilder(ProducerBuilder&&)      = default;
    ~ProducerBuilder()                      = default;
    ProducerBuilder& operator=(const ProducerBuilder&) = default;
    ProducerBuilder& operator=(ProducerBuilder&&) = default;

    ProducerBuilder& name(std::string name) &;
    ProducerBuilder& stream(std::string stream) &;
    ProducerBuilder& batch_size(std::uint32_t batch_size) &;
    ProducerBuilder& batch_publishing_delay(std::chrono::milliseconds batch_publishing_delay) &;
    ProducerBuilder& max_unconfirmed(std::uint32_t max_unconfirmed) &;
    ProducerBuilder& confirm_timeout(std::chrono::milliseconds confirm_timeout) &;
    ProducerBuilder& enqueue_timeout(std::chrono::milliseconds enqueue_timeout) &;

    ProducerBuilder name(std::string name) &&;
    ProducerBuilder stream(std::string stream) &&;
    ProducerBuilder batch_size(std::uint32_t batch_size) &&;
    ProducerBuilder batch_publishing_delay(std::chrono::milliseconds batch_publishing_delay) &&;
    ProducerBuilder max_unconfirmed(std::uint32_t max_unconfirmed) &&;
    ProducerBuilder confirm_timeout(std::chrono::milliseconds confirm_timeout) &&;
    ProducerBuilder enqueue_timeout(std::chrono::milliseconds enqueue_timeout) &&;

    ProducerPtr build() const&;
    ProducerPtr build() &&;

private:
    detail::InternalEnvironmentPtr m_environment            = {};
    std::string                    m_name                   = {};
    std::string                    m_stream                 = {};
    std::uint32_t                  m_batch_size             = 100;
    std::chrono::milliseconds      m_batch_publishing_delay = std::chrono::milliseconds(100);
    std::uint32_t                  m_max_unconfirmed        = 10'000;
    std::chrono::milliseconds      m_confirm_timeout        = std::chrono::seconds(30);
    std::chrono::milliseconds      m_enqueue_timeout        = std::chrono::seconds(10);
};

}  // namespace hareflow