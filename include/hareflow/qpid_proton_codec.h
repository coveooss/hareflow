#pragma once

#include <cstddef>

#include "hareflow/codec.h"

namespace hareflow {

class QpidProtonCodec : public Codec
{
public:
    HAREFLOW_EXPORT QpidProtonCodec()          = default;
    HAREFLOW_EXPORT virtual ~QpidProtonCodec() = default;

    std::vector<std::uint8_t> encode(MessagePtr message) override;
    MessagePtr                decode(const void* data, std::size_t data_size) override;
};

}  // namespace hareflow