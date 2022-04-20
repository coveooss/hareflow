#pragma once

#include <cstdint>

#include "hareflow/types.h"

namespace hareflow {

class HAREFLOW_EXPORT Consumer
{
public:
    virtual ~Consumer() = default;

    virtual void store_offset(std::uint64_t offset) = 0;
    virtual void grant_credit()                     = 0;
};

}  // namespace hareflow