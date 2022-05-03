#include <gmock/gmock.h>

#include "hareflow/detail/consumer_impl.h"

namespace hareflow::tests {

class InternalConsumerMock : public detail::InternalConsumer
{
public:
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));

    MOCK_METHOD(void, store_offset, (std::uint64_t offset), (override));
    MOCK_METHOD(void, grant_credit, (), (override));
};

}  // namespace hareflow::tests