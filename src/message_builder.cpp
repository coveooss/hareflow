#include "hareflow/message_builder.h"

#include "hareflow/exceptions.h"

namespace hareflow {

MessageBuilder& MessageBuilder::body(std::vector<std::uint8_t> data) &
{
    m_body = std::move(data);
    return *this;
}

MessageBuilder& MessageBuilder::body(std::string_view data) &
{
    static_assert(sizeof(char) == sizeof(std::uint8_t));
    const std::uint8_t* data_start = reinterpret_cast<const std::uint8_t*>(data.data());
    m_body.assign(data_start, data_start + data.size());
    return *this;
}

MessageBuilder& MessageBuilder::body(const void* data, std::size_t data_size) &
{
    const std::uint8_t* data_start = reinterpret_cast<const std::uint8_t*>(data);
    m_body.assign(data_start, data_start + data_size);
    return *this;
}

MessageBuilder& MessageBuilder::headers(std::map<std::string, std::string> headers) &
{
    m_headers = std::move(headers);
    return *this;
}

MessageBuilder MessageBuilder::body(std::vector<std::uint8_t> data) &&
{
    return std::move(this->body(std::move(data)));
}

MessageBuilder MessageBuilder::body(std::string_view data) &&
{
    return std::move(this->body(std::move(data)));
}

MessageBuilder MessageBuilder::body(const void* data, std::size_t data_size) &&
{
    return std::move(this->body(data, data_size));
}

MessageBuilder MessageBuilder::headers(std::map<std::string, std::string> headers) &&
{
    return std::move(this->headers(std::move(headers)));
}

MessagePtr MessageBuilder::build() const&
{
    return std::make_shared<Message>(m_body, m_headers);
}

MessagePtr MessageBuilder::build() &&
{
    return std::make_shared<Message>(std::move(m_body), std::move(m_headers));
}

}  // namespace hareflow