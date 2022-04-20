#pragma once

#include <cstdint>

#include "hareflow/types.h"

namespace hareflow {

class HAREFLOW_EXPORT Producer
{
public:
    virtual ~Producer() = default;

    virtual void send(MessagePtr message, ConfirmationHandler confirmation_handler)                              = 0;
    virtual void send(std::uint64_t publishing_id, MessagePtr message, ConfirmationHandler confirmation_handler) = 0;
    virtual void flush()                                                                                         = 0;
};

}  // namespace hareflow