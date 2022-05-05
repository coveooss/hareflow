#pragma once

#include <optional>
#include <string>

#include "hareflow/consumer.h"
#include "hareflow/types.h"

namespace hareflow {

class HAREFLOW_EXPORT ConsumerBuilder
{
public:
    ConsumerBuilder(detail::InternalEnvironmentPtr environment);
    ConsumerBuilder(const ConsumerBuilder&) = default;
    ConsumerBuilder(ConsumerBuilder&&)      = default;
    ~ConsumerBuilder()                      = default;
    ConsumerBuilder& operator=(const ConsumerBuilder&) = default;
    ConsumerBuilder& operator=(ConsumerBuilder&&) = default;

    ConsumerBuilder& name(std::string name) &;
    ConsumerBuilder& stream(std::string stream) &;
    ConsumerBuilder& offset_specification(OffsetSpecification offset_specification) &;
    ConsumerBuilder& manual_cursor_management() &;
    ConsumerBuilder& automatic_cursor_management(AutomaticCursorConfiguration cursor_configuration) &;
    ConsumerBuilder& manual_credit_management() &;
    ConsumerBuilder& automatic_credit_management() &;
    ConsumerBuilder& message_handler(MessageHandler message_handler) &;

    ConsumerBuilder name(std::string name) &&;
    ConsumerBuilder stream(std::string stream) &&;
    ConsumerBuilder offset_specification(OffsetSpecification offset_specification) &&;
    ConsumerBuilder manual_cursor_management() &&;
    ConsumerBuilder automatic_cursor_management(AutomaticCursorConfiguration cursor_configuration) &&;
    ConsumerBuilder manual_credit_management() &&;
    ConsumerBuilder automatic_credit_management() &&;
    ConsumerBuilder message_handler(MessageHandler message_handler) &&;

    ConsumerPtr build() const&;
    ConsumerPtr build() &&;

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