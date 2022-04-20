#pragma once

#include <cstdint>

#include <chrono>
#include <memory>
#include <string>

#include "hareflow/types.h"
#include "hareflow/producer.h"

namespace hareflow {

class ProducerBuilder
{
public:
    ProducerBuilder(detail::InternalEnvironmentPtr environment);
    HAREFLOW_EXPORT ProducerBuilder(const ProducerBuilder&) = default;
    HAREFLOW_EXPORT ProducerBuilder(ProducerBuilder&&)      = default;
    HAREFLOW_EXPORT ~ProducerBuilder()                      = default;
    HAREFLOW_EXPORT ProducerBuilder& operator=(const ProducerBuilder&) = default;
    HAREFLOW_EXPORT ProducerBuilder& operator=(ProducerBuilder&&) = default;

    HAREFLOW_EXPORT ProducerBuilder& name(std::string name) &;
    HAREFLOW_EXPORT ProducerBuilder& stream(std::string stream) &;
    HAREFLOW_EXPORT ProducerBuilder& batch_size(std::uint32_t batch_size) &;
    HAREFLOW_EXPORT ProducerBuilder& batch_publishing_delay(std::chrono::milliseconds batch_publishing_delay) &;
    HAREFLOW_EXPORT ProducerBuilder& max_unconfirmed(std::uint32_t max_unconfirmed) &;
    HAREFLOW_EXPORT ProducerBuilder& confirm_timeout(std::chrono::milliseconds confirm_timeout) &;
    HAREFLOW_EXPORT ProducerBuilder& enqueue_timeout(std::chrono::milliseconds enqueue_timeout) &;

    HAREFLOW_EXPORT ProducerBuilder name(std::string name) &&;
    HAREFLOW_EXPORT ProducerBuilder stream(std::string stream) &&;
    HAREFLOW_EXPORT ProducerBuilder batch_size(std::uint32_t batch_size) &&;
    HAREFLOW_EXPORT ProducerBuilder batch_publishing_delay(std::chrono::milliseconds batch_publishing_delay) &&;
    HAREFLOW_EXPORT ProducerBuilder max_unconfirmed(std::uint32_t max_unconfirmed) &&;
    HAREFLOW_EXPORT ProducerBuilder confirm_timeout(std::chrono::milliseconds confirm_timeout) &&;
    HAREFLOW_EXPORT ProducerBuilder enqueue_timeout(std::chrono::milliseconds enqueue_timeout) &&;

    HAREFLOW_EXPORT ProducerPtr build() const&;
    HAREFLOW_EXPORT ProducerPtr build() &&;

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