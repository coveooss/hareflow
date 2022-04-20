#pragma once

#include <string>

#include "hareflow/consumer_builder.h"
#include "hareflow/producer_builder.h"
#include "hareflow/stream_creator.h"
#include "hareflow/types.h"

namespace hareflow {

class HAREFLOW_EXPORT Environment
{
public:
    virtual ~Environment() = default;

    virtual ProducerBuilder producer_builder()                     = 0;
    virtual ConsumerBuilder consumer_builder()                     = 0;
    virtual StreamCreator   stream_creator()                       = 0;
    virtual void            delete_stream(std::string_view stream) = 0;
};

}  // namespace hareflow