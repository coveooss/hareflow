#pragma once

#include <optional>
#include <string>

#include "hareflow/consumer.h"
#include "hareflow/types.h"

namespace hareflow {

class ConsumerBuilder
{
public:
    ConsumerBuilder(detail::InternalEnvironmentPtr environment);
    HAREFLOW_EXPORT ConsumerBuilder(const ConsumerBuilder&) = default;
    HAREFLOW_EXPORT ConsumerBuilder(ConsumerBuilder&&)      = default;
    HAREFLOW_EXPORT ~ConsumerBuilder()                      = default;
    HAREFLOW_EXPORT ConsumerBuilder& operator=(const ConsumerBuilder&) = default;
    HAREFLOW_EXPORT ConsumerBuilder& operator=(ConsumerBuilder&&) = default;

    HAREFLOW_EXPORT ConsumerBuilder& name(std::string name) &;
    HAREFLOW_EXPORT ConsumerBuilder& stream(std::string stream) &;
    HAREFLOW_EXPORT ConsumerBuilder& offset_specification(OffsetSpecification offset_specification) &;
    HAREFLOW_EXPORT ConsumerBuilder& manual_cursor_management() &;
    HAREFLOW_EXPORT ConsumerBuilder& automatic_cursor_management(AutomaticCursorConfiguration cursor_configuration) &;
    HAREFLOW_EXPORT ConsumerBuilder& manual_credit_management() &;
    HAREFLOW_EXPORT ConsumerBuilder& automatic_credit_management() &;
    HAREFLOW_EXPORT ConsumerBuilder& message_handler(MessageHandler message_handler) &;

    HAREFLOW_EXPORT ConsumerBuilder name(std::string name) &&;
    HAREFLOW_EXPORT ConsumerBuilder stream(std::string stream) &&;
    HAREFLOW_EXPORT ConsumerBuilder offset_specification(OffsetSpecification offset_specification) &&;
    HAREFLOW_EXPORT ConsumerBuilder manual_cursor_management() &&;
    HAREFLOW_EXPORT ConsumerBuilder automatic_cursor_management(AutomaticCursorConfiguration cursor_configuration) &&;
    HAREFLOW_EXPORT ConsumerBuilder manual_credit_management() &&;
    HAREFLOW_EXPORT ConsumerBuilder automatic_credit_management() &&;
    HAREFLOW_EXPORT ConsumerBuilder message_handler(MessageHandler message_handler) &&;

    HAREFLOW_EXPORT ConsumerPtr build() const&;
    HAREFLOW_EXPORT ConsumerPtr build() &&;

private:
    detail::InternalEnvironmentPtr              m_environment          = {};
    std::string                                 m_name                 = {};
    std::string                                 m_stream               = {};
    OffsetSpecification                         m_offset_specification = OffsetSpecification::next();
    std::optional<AutomaticCursorConfiguration> m_auto_cursor_config   = AutomaticCursorConfiguration{};
    bool                                        m_automatic_credits    = true;
    MessageHandler                              m_message_handler      = {};
};

}  // namespace hareflow