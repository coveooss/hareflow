#pragma once

#include <vector>

#include "hareflow/types.h"

namespace hareflow {

class Codec
{
public:
    HAREFLOW_EXPORT virtual ~Codec() = default;

    virtual std::vector<std::uint8_t> encode(MessagePtr message)                      = 0;
    virtual MessagePtr                decode(const void* data, std::size_t data_size) = 0;
};

class RawCodec : public Codec
{
public:
    HAREFLOW_EXPORT RawCodec()          = default;
    HAREFLOW_EXPORT virtual ~RawCodec() = default;

    std::vector<std::uint8_t> encode(MessagePtr message) override;
    MessagePtr                decode(const void* data, std::size_t data_size) override;
};

}  // namespace hareflow