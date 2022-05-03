#include "hareflow/consumer_builder.h"

#include "hareflow/exceptions.h"
#include "hareflow/detail/consumer_impl.h"

namespace hareflow {

ConsumerBuilder::ConsumerBuilder(detail::InternalEnvironmentPtr environment) : m_environment(environment)
{
}

ConsumerBuilder& ConsumerBuilder::name(std::string name) &
{
    if (name.size() >= 256) {
        throw InvalidInputException("name length must not exceed 256");
    }
    m_name = std::move(name);
    return *this;
}

ConsumerBuilder& ConsumerBuilder::stream(std::string stream) &
{
    m_stream = std::move(stream);
    return *this;
}

ConsumerBuilder& ConsumerBuilder::offset_specification(OffsetSpecification offset_specification) &
{
    m_offset_specification = std::move(offset_specification);
    return *this;
}

ConsumerBuilder& ConsumerBuilder::manual_cursor_management() &
{
    m_auto_cursor_config = std::nullopt;
    return *this;
}

ConsumerBuilder& ConsumerBuilder::automatic_cursor_management(AutomaticCursorConfiguration cursor_configuration) &
{
    if (cursor_configuration.force_persist_delay() < std::chrono::milliseconds::zero()) {
        throw InvalidInputException("force persist delay must not be negative");
    }
    if (cursor_configuration.persist_frequency() == 0 && cursor_configuration.force_persist_delay() == std::chrono::milliseconds::zero()) {
        throw InvalidInputException("Automatic cursor management requires a either a message frequency or persist delay");
    }
    m_auto_cursor_config = std::move(cursor_configuration);
    return *this;
}

ConsumerBuilder& ConsumerBuilder::manual_credit_management() &
{
    m_automatic_credits = false;
    return *this;
}

ConsumerBuilder& ConsumerBuilder::automatic_credit_management() &
{
    m_automatic_credits = true;
    return *this;
}

ConsumerBuilder& ConsumerBuilder::message_handler(MessageHandler message_handler) &
{
    m_message_handler = std::move(message_handler);
    return *this;
}

ConsumerBuilder ConsumerBuilder::name(std::string name) &&
{
    return std::move(this->name(std::move(name)));
}

ConsumerBuilder ConsumerBuilder::stream(std::string stream) &&
{
    return std::move(this->stream(std::move(stream)));
}

ConsumerBuilder ConsumerBuilder::offset_specification(OffsetSpecification offset_specification) &&
{
    return std::move(this->offset_specification(std::move(offset_specification)));
}

ConsumerBuilder ConsumerBuilder::manual_cursor_management() &&
{
    return std::move(this->manual_cursor_management());
}

ConsumerBuilder ConsumerBuilder::automatic_cursor_management(AutomaticCursorConfiguration cursor_configuration) &&
{
    return std::move(this->automatic_cursor_management(std::move(cursor_configuration)));
}

ConsumerBuilder ConsumerBuilder::manual_credit_management() &&
{
    return std::move(this->manual_credit_management());
}

ConsumerBuilder ConsumerBuilder::automatic_credit_management() &&
{
    return std::move(this->automatic_credit_management());
}

ConsumerBuilder ConsumerBuilder::message_handler(MessageHandler message_handler) &&
{
    return std::move(this->message_handler(std::move(message_handler)));
}

ConsumerPtr ConsumerBuilder::build() const&
{
    if (m_stream.empty()) {
        throw InvalidInputException("stream must be specified");
    }
    auto consumer =
        detail::ConsumerImpl::create(m_name, m_stream, m_offset_specification, m_auto_cursor_config, m_automatic_credits, m_message_handler, m_environment);
    consumer->start();
    return consumer;
}

ConsumerPtr ConsumerBuilder::build() &&
{
    if (m_stream.empty()) {
        throw InvalidInputException("stream must be specified");
    }
    auto consumer = detail::ConsumerImpl::create(std::move(m_name),
                                                 std::move(m_stream),
                                                 std::move(m_offset_specification),
                                                 std::move(m_auto_cursor_config),
                                                 m_automatic_credits,
                                                 std::move(m_message_handler),
                                                 std::move(m_environment));
    consumer->start();
    return consumer;
}

}  // namespace hareflow