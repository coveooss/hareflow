#include <gmock/gmock.h>

#include "hareflow/detail/environment_impl.h"

namespace hareflow::tests {

class InternalEnvironmentMock : public detail::InternalEnvironment
{
public:
    MOCK_METHOD(void, register_producer, (detail::InternalProducerWeakPtr producer), (override));
    MOCK_METHOD(detail::InternalClientPtr, get_producer_client, (std::string_view stream, ClientParameters parameters), (override));
    MOCK_METHOD(void, unregister_producer, (detail::InternalProducerWeakPtr producer), (override));

    MOCK_METHOD(void, register_consumer, (detail::InternalConsumerWeakPtr consumer), (override));
    MOCK_METHOD(detail::InternalClientPtr, get_consumer_client, (std::string_view stream, ClientParameters parameters), (override));
    MOCK_METHOD(void, unregister_consumer, (detail::InternalConsumerWeakPtr consumer), (override));

    MOCK_METHOD(std::chrono::milliseconds, get_recovery_retry_delay, (), (override));
    MOCK_METHOD(ClientParameters, get_base_client_parameters, (), (override));
    MOCK_METHOD(CodecPtr, get_codec, (), (override));
    MOCK_METHOD(detail::BackgroundScheduler&, background_scheduler, (), (override));
    MOCK_METHOD(void, locator_operation, (std::function<void(Client&)> operation), (override));

    MOCK_METHOD(ProducerBuilder, producer_builder, (), (override));
    MOCK_METHOD(ConsumerBuilder, consumer_builder, (), (override));
    MOCK_METHOD(StreamCreator, stream_creator, (), (override));
    MOCK_METHOD(void, delete_stream, (std::string_view stream), (override));
};

}  // namespace hareflow::tests