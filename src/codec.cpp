#include "hareflow/codec.h"

#include "hareflow/exceptions.h"
#include "hareflow/message_builder.h"

namespace hareflow {

std::vector<std::uint8_t> RawCodec::encode(MessagePtr message)
{
    if (!message->headers().empty()) {
        throw InvalidInputException("headers not supported for RawCodec");
    }
    return message->body();
}

MessagePtr RawCodec::decode(const void* data, std::size_t data_size)
{
    return MessageBuilder().body(data, data_size).build();
}

}  // namespace hareflow