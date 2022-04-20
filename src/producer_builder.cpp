#include "hareflow/producer_builder.h"

#include "hareflow/exceptions.h"
#include "hareflow/detail/producer_impl.h"

namespace hareflow {

ProducerBuilder::ProducerBuilder(detail::InternalEnvironmentPtr environment) : m_environment(environment)
{
}

ProducerBuilder& ProducerBuilder::name(std::string name) &
{
    if (name.size() >= 256) {
        throw InvalidInputException("name length must not exceed 256");
    }
    m_name = std::move(name);
    return *this;
}

ProducerBuilder& ProducerBuilder::stream(std::string stream) &
{
    m_stream = std::move(stream);
    return *this;
}

ProducerBuilder& ProducerBuilder::batch_size(std::uint32_t batch_size) &
{
    if (batch_size == 0) {
        throw InvalidInputException("batch size must be greater than zero");
    }
    m_batch_size = batch_size;
    return *this;
}

ProducerBuilder& ProducerBuilder::batch_publishing_delay(std::chrono::milliseconds batch_publishing_delay) &
{
    if (batch_publishing_delay < std::chrono::milliseconds::zero()) {
        throw InvalidInputException("batch publishing delay must not be negative");
    }
    m_batch_publishing_delay = batch_publishing_delay;
    return *this;
}

ProducerBuilder& ProducerBuilder::max_unconfirmed(std::uint32_t max_unconfirmed) &
{
    if (max_unconfirmed == 0) {
        throw InvalidInputException("max unconfirmed must be greater than zero");
    }
    m_max_unconfirmed = max_unconfirmed;
    return *this;
}

ProducerBuilder& ProducerBuilder::confirm_timeout(std::chrono::milliseconds confirm_timeout) &
{
    if (confirm_timeout < std::chrono::milliseconds::zero()) {
        throw InvalidInputException("confirm timeout must not be negative");
    }
    m_confirm_timeout = confirm_timeout;
    return *this;
}

ProducerBuilder& ProducerBuilder::enqueue_timeout(std::chrono::milliseconds enqueue_timeout) &
{
    if (enqueue_timeout < std::chrono::milliseconds::zero()) {
        throw InvalidInputException("enqueue timeout must not be negative");
    }
    m_enqueue_timeout = enqueue_timeout;
    return *this;
}

ProducerBuilder ProducerBuilder::name(std::string name) &&
{
    return std::move(this->name(std::move(name)));
}

ProducerBuilder ProducerBuilder::stream(std::string stream) &&
{
    return std::move(this->stream(std::move(stream)));
}

ProducerBuilder ProducerBuilder::batch_size(std::uint32_t batch_size) &&
{
    return std::move(this->batch_size(batch_size));
}

ProducerBuilder ProducerBuilder::batch_publishing_delay(std::chrono::milliseconds batch_publishing_delay) &&
{
    return std::move(this->batch_publishing_delay(batch_publishing_delay));
}

ProducerBuilder ProducerBuilder::max_unconfirmed(std::uint32_t max_unconfirmed) &&
{
    return std::move(this->max_unconfirmed(max_unconfirmed));
}

ProducerBuilder ProducerBuilder::confirm_timeout(std::chrono::milliseconds confirm_timeout) &&
{
    return std::move(this->confirm_timeout(confirm_timeout));
}

ProducerBuilder ProducerBuilder::enqueue_timeout(std::chrono::milliseconds enqueue_timeout) &&
{
    return std::move(this->enqueue_timeout(enqueue_timeout));
}

ProducerPtr ProducerBuilder::build() const&
{
    if (m_stream.empty()) {
        throw InvalidInputException("stream must be specified");
    }
    auto producer = detail::ProducerImpl::create(m_name,
                                                 m_stream,
                                                 m_batch_size,
                                                 m_batch_publishing_delay,
                                                 m_max_unconfirmed,
                                                 m_confirm_timeout,
                                                 m_enqueue_timeout,
                                                 m_environment);
    producer->start();
    return producer;
}

ProducerPtr ProducerBuilder::build() &&
{
    if (m_stream.empty()) {
        throw InvalidInputException("stream must be specified");
    }
    auto producer = detail::ProducerImpl::create(std::move(m_name),
                                                 std::move(m_stream),
                                                 m_batch_size,
                                                 m_batch_publishing_delay,
                                                 m_max_unconfirmed,
                                                 m_confirm_timeout,
                                                 m_enqueue_timeout,
                                                 std::move(m_environment));
    producer->start();
    return producer;
}

}  // namespace hareflow