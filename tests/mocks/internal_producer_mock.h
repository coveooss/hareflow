#include <gmock/gmock.h>

#include "hareflow/detail/producer_impl.h"

namespace hareflow::tests {

class InternalProducerMock : public detail::InternalProducer
{
public:
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));

    MOCK_METHOD(void, send, (MessagePtr message, ConfirmationHandler confirmation_handler), (override));
    MOCK_METHOD(void, send, (std::uint64_t publishing_id, MessagePtr message, ConfirmationHandler confirmation_handler), (override));
    MOCK_METHOD(void, flush, (), (override));
};

}  // namespace hareflow::tests