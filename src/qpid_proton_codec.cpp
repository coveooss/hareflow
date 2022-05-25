#include "hareflow/qpid_proton_codec.h"

#include <fmt/core.h>

#ifdef _WIN32
#    pragma warning(push)
#    pragma warning(disable : 4702)
#endif

#include <proton/error.hpp>
#include <proton/message.hpp>
#include <proton/types.hpp>

#ifdef _WIN32
#    pragma warning(pop)
#endif

#include "hareflow/exceptions.h"
#include "hareflow/message_builder.h"

namespace hareflow {

std::vector<std::uint8_t> QpidProtonCodec::encode(MessagePtr message)
{
    static thread_local std::vector<char> encoded_chars;
    static thread_local proton::message   amqp_message;
    encoded_chars.clear();
    amqp_message.clear();

    amqp_message.body() = proton::binary(message->body());
    if (!message->headers().empty()) {
        amqp_message.properties() = std::map<std::string, proton::scalar>(message->headers().begin(), message->headers().end());
    }
    amqp_message.encode(encoded_chars);

    static_assert(sizeof(char) == sizeof(std::uint8_t));
    const std::uint8_t* data_start = reinterpret_cast<const std::uint8_t*>(encoded_chars.data());
    return std::vector<std::uint8_t>(data_start, data_start + encoded_chars.size());
}

MessagePtr QpidProtonCodec::decode(const void* data, std::size_t data_size)
{
    static thread_local std::vector<char> encoded_chars;
    static thread_local proton::message   amqp_message;
    encoded_chars.clear();
    amqp_message.clear();

    try {
        const char* chars_start = reinterpret_cast<const char*>(data);
        encoded_chars.assign(chars_start, chars_start + data_size);
        amqp_message.decode(encoded_chars);

        auto body    = proton::coerce<proton::binary>(amqp_message.body());
        auto headers = proton::coerce<std::map<std::string, std::string>>(amqp_message.properties().value());

        return MessageBuilder().body(std::move(body)).headers(std::move(headers)).build();
    } catch (const proton::error& e) {
        throw StreamException(fmt::format("Failed to decode message: {}", e.what()));
    }
}

}  // namespace hareflow