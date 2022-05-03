#include <chrono>
#include <future>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "hareflow.h"
#include "hareflow/detail/internal_types.h"
#include "hareflow/detail/consumer_impl.h"
#include "mocks/internal_client_mock.h"
#include "mocks/internal_environment_mock.h"

constexpr std::string_view stream_name   = "my-stream";
constexpr std::string_view consumer_name = "my-consumer";

class ConsumerTest : public testing::Test
{
protected:
    ConsumerTest()
    {
        m_internal_environment = hareflow::detail::InternalEnvironmentPtr(&m_environment_mock, [](auto...) {});
        m_internal_client      = hareflow::detail::InternalClientPtr(&m_client_mock, [](auto...) {});
    }

    void SetUp() override
    {
        ON_CALL(m_environment_mock, background_scheduler()).WillByDefault(testing::ReturnRef(m_scheduler));
        ON_CALL(m_environment_mock, get_recovery_retry_delay()).WillByDefault(testing::Return(std::chrono::milliseconds{1}));
        ON_CALL(m_environment_mock, get_base_client_parameters()).WillByDefault(testing::Return(hareflow::ClientParameters{}));
        ON_CALL(m_environment_mock, get_consumer_client(stream_name, testing::_)).WillByDefault(testing::Return(m_internal_client));
    }

    hareflow::ConsumerBuilder consumer_builder()
    {
        return hareflow::ConsumerBuilder{m_internal_environment}.stream(std::string{stream_name});
    }

    hareflow::detail::BackgroundScheduler                       m_scheduler;
    testing::NiceMock<hareflow::tests::InternalEnvironmentMock> m_environment_mock;
    testing::NiceMock<hareflow::tests::InternalClientMock>      m_client_mock;
    hareflow::detail::InternalEnvironmentPtr                    m_internal_environment;
    hareflow::detail::InternalClientPtr                         m_internal_client;
};

TEST_F(ConsumerTest, BasicStartStop)
{
    testing::InSequence s;

    EXPECT_CALL(m_environment_mock, register_consumer(testing::_));
    EXPECT_CALL(m_client_mock, start());
    EXPECT_CALL(m_client_mock, subscribe(testing::_, stream_name, testing::_, testing::_, testing::_));
    EXPECT_CALL(m_client_mock, unsubscribe(testing::_));
    EXPECT_CALL(m_client_mock, stop());
    EXPECT_CALL(m_environment_mock, unregister_consumer(testing::_));

    hareflow::ConsumerPtr consumer = consumer_builder().build();
}

TEST_F(ConsumerTest, PartiallyConsumedChunk)
{
    static constexpr std::uint64_t chunk_start_offset = 3;
    static constexpr std::uint64_t chunk_end_offset   = 6;
    static constexpr std::uint64_t consume_offset     = 5;

    hareflow::ClientParameters client_parameters;
    EXPECT_CALL(m_environment_mock, get_consumer_client(stream_name, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)));
    EXPECT_CALL(m_client_mock, subscribe(testing::_, stream_name, hareflow::OffsetSpecification::offset(consume_offset), testing::_, testing::_));

    int                   consumed = 0;
    hareflow::ConsumerPtr consumer = consumer_builder()
                                         .offset_specification(hareflow::OffsetSpecification::offset(consume_offset))
                                         .message_handler([&](auto& context, auto message) {
                                             EXPECT_GE(context.offset, consume_offset);
                                             ++consumed;
                                         })
                                         .build();

    for (auto offset = chunk_start_offset; offset <= chunk_end_offset; ++offset) {
        client_parameters.get_message_listener()(0, 0, offset, hareflow::MessageBuilder().body("hello").build());
    }

    EXPECT_EQ(consumed, 2);
}

TEST_F(ConsumerTest, NamedConsumerStoredOffset)
{
    EXPECT_CALL(m_client_mock, query_offset(consumer_name, stream_name)).WillOnce(testing::Return(2));
    EXPECT_CALL(m_client_mock, subscribe(testing::_, stream_name, hareflow::OffsetSpecification::offset(3), testing::_, testing::_));

    hareflow::ConsumerPtr consumer = consumer_builder().name(std::string{consumer_name}).offset_specification(hareflow::OffsetSpecification::first()).build();
}

TEST_F(ConsumerTest, NamedConsumerFallbackOffset)
{
    EXPECT_CALL(m_client_mock, query_offset(consumer_name, stream_name)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(m_client_mock, subscribe(testing::_, stream_name, hareflow::OffsetSpecification::first(), testing::_, testing::_));

    hareflow::ConsumerPtr consumer = consumer_builder().name(std::string{consumer_name}).offset_specification(hareflow::OffsetSpecification::first()).build();
}

TEST_F(ConsumerTest, AutoCursorFrequency)
{
    static constexpr std::uint64_t to_consume = 5;

    std::promise<void>         done;
    hareflow::ClientParameters client_parameters;
    EXPECT_CALL(m_environment_mock, get_consumer_client(stream_name, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)));
    EXPECT_CALL(m_client_mock, query_offset(consumer_name, stream_name)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(m_client_mock, store_offset(consumer_name, stream_name, to_consume - 1)).WillOnce(testing::Invoke([&]() { done.set_value(); }));

    int                   consumed = 0;
    hareflow::ConsumerPtr consumer = consumer_builder()
                                         .name(std::string{consumer_name})
                                         .automatic_cursor_management(hareflow::AutomaticCursorConfiguration{to_consume, std::chrono::seconds::zero()})
                                         .message_handler([&](auto...) { ++consumed; })
                                         .build();

    for (int i = 0; i < to_consume; ++i) {
        client_parameters.get_message_listener()(0, 0, i, hareflow::MessageBuilder().body("hello").build());
    }

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds::zero()), std::future_status::ready);
    EXPECT_EQ(consumed, to_consume);
}

TEST_F(ConsumerTest, AutoCursorForceDelay)
{
    std::promise<void>         done;
    hareflow::ClientParameters client_parameters;
    EXPECT_CALL(m_environment_mock, get_consumer_client(stream_name, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)));
    EXPECT_CALL(m_client_mock, query_offset(consumer_name, stream_name)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(m_client_mock, store_offset(consumer_name, stream_name, 1)).WillOnce(testing::Invoke([&]() { done.set_value(); }));

    int                   consumed = 0;
    hareflow::ConsumerPtr consumer = consumer_builder()
                                         .name(std::string{consumer_name})
                                         .automatic_cursor_management(hareflow::AutomaticCursorConfiguration{0, std::chrono::milliseconds{1}})
                                         .message_handler([&](auto...) { ++consumed; })
                                         .build();

    client_parameters.get_message_listener()(0, 0, 1, hareflow::MessageBuilder().body("hello").build());

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
    EXPECT_EQ(consumed, 1);
}

TEST_F(ConsumerTest, AutoCursorStoreOnStop)
{
    std::promise<void>         done;
    hareflow::ClientParameters client_parameters;
    EXPECT_CALL(m_environment_mock, get_consumer_client(stream_name, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)));
    EXPECT_CALL(m_client_mock, query_offset(consumer_name, stream_name)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(m_client_mock, store_offset(consumer_name, stream_name, 1)).WillOnce(testing::Invoke([&]() { done.set_value(); }));

    int                   consumed = 0;
    hareflow::ConsumerPtr consumer = consumer_builder()
                                         .name(std::string{consumer_name})
                                         .automatic_cursor_management(hareflow::AutomaticCursorConfiguration{100, std::chrono::milliseconds::zero()})
                                         .message_handler([&](auto...) { ++consumed; })
                                         .build();

    client_parameters.get_message_listener()(0, 0, 1, hareflow::MessageBuilder().body("hello").build());

    consumer = nullptr;

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds::zero()), std::future_status::ready);
    EXPECT_EQ(consumed, 1);
}

TEST_F(ConsumerTest, ReconnectRetries)
{
    std::promise<void>         done;
    hareflow::ClientParameters client_parameters;
    EXPECT_CALL(m_environment_mock, get_consumer_client(stream_name, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)))
        .WillRepeatedly(testing::Return(m_internal_client));
    EXPECT_CALL(m_client_mock, start())
        .WillOnce(testing::Return())
        .WillOnce(testing::Throw(hareflow::IOException("Failed to connect")))
        .WillOnce(testing::Return());
    EXPECT_CALL(m_client_mock, subscribe(testing::_, stream_name, testing::_, testing::_, testing::_))
        .WillOnce(testing::Return())
        .WillOnce(testing::Invoke([&]() { done.set_value(); }));

    hareflow::ConsumerPtr consumer = consumer_builder().build();

    client_parameters.get_shutdown_listener()(hareflow::ShutdownReason::Unknown);

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
}

TEST_F(ConsumerTest, ReconnectNoLastSeen)
{
    std::promise<void>         done;
    hareflow::ClientParameters client_parameters;
    EXPECT_CALL(m_environment_mock, get_consumer_client(stream_name, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)))
        .WillRepeatedly(testing::Return(m_internal_client));
    EXPECT_CALL(m_client_mock, subscribe(testing::_, stream_name, hareflow::OffsetSpecification::next(), testing::_, testing::_))
        .WillOnce(testing::Return())
        .WillOnce(testing::Invoke([&]() { done.set_value(); }));

    hareflow::ConsumerPtr consumer = consumer_builder().offset_specification(hareflow::OffsetSpecification::next()).build();

    client_parameters.get_shutdown_listener()(hareflow::ShutdownReason::Unknown);

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
}

TEST_F(ConsumerTest, ReconnectWithLastSeen)
{
    std::promise<void>         done;
    hareflow::ClientParameters client_parameters;
    EXPECT_CALL(m_environment_mock, get_consumer_client(stream_name, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)))
        .WillRepeatedly(testing::Return(m_internal_client));
    testing::Expectation initial_subscribe =
        EXPECT_CALL(m_client_mock, subscribe(testing::_, stream_name, hareflow::OffsetSpecification::next(), testing::_, testing::_));
    EXPECT_CALL(m_client_mock, subscribe(testing::_, stream_name, hareflow::OffsetSpecification::offset(2), testing::_, testing::_))
        .After(initial_subscribe)
        .WillOnce(testing::Invoke([&]() { done.set_value(); }));

    hareflow::ConsumerPtr consumer = consumer_builder().offset_specification(hareflow::OffsetSpecification::next()).build();

    client_parameters.get_message_listener()(0, 0, 1, hareflow::MessageBuilder().body("hello").build());
    client_parameters.get_shutdown_listener()(hareflow::ShutdownReason::Unknown);

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
}

TEST_F(ConsumerTest, StreamDeleted)
{
    std::promise<void>         done;
    hareflow::ClientParameters client_parameters;
    EXPECT_CALL(m_environment_mock, get_consumer_client(testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)))
        .WillOnce(testing::Throw(hareflow::ResponseErrorException(hareflow::ResponseCode::StreamDoesNotExist)));
    EXPECT_CALL(m_environment_mock, unregister_consumer(testing::_)).WillOnce(testing::Invoke([&]() { done.set_value(); }));

    hareflow::ConsumerPtr consumer = consumer_builder().build();

    client_parameters.get_metadata_listener()(stream_name, hareflow::ResponseCode::StreamNotAvailable);

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
}