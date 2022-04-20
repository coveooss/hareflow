#pragma once

#include <memory>

#include "hareflow/types.h"

namespace hareflow {

namespace detail {

class Connection;
class ClientRequest;
class ServerResponse;
class BinaryBuffer;
class Accumulator;
class InternalClient;
class InternalProducer;
class InternalConsumer;
struct AccumulatedMessage;

using InternalClientPtr       = std::shared_ptr<InternalClient>;
using InternalProducerPtr     = std::shared_ptr<InternalProducer>;
using InternalProducerWeakPtr = std::weak_ptr<InternalProducer>;
using InternalConsumerPtr     = std::shared_ptr<InternalConsumer>;
using InternalConsumerWeakPtr = std::weak_ptr<InternalConsumer>;
using AccumulatedMessagePtr   = std::shared_ptr<AccumulatedMessage>;

}  // namespace detail

}  // namespace hareflow