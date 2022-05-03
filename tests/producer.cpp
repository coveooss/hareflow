#include <chrono>
#include <future>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "hareflow.h"
#include "hareflow/detail/internal_types.h"
#include "hareflow/detail/producer_impl.h"
#include "mocks/internal_client_mock.h"
#include "mocks/internal_environment_mock.h"

constexpr std::string_view stream_name    = "my-stream";
constexpr std::string_view publisher_name = "my-publisher";

class ProducerTest : public testing::Test
{
protected:
    ProducerTest()
    {
        m_internal_environment = hareflow::detail::InternalEnvironmentPtr(&m_environment_mock, [](auto...) {});
        m_internal_client      = hareflow::detail::InternalClientPtr(&m_client_mock, [](auto...) {});
    }

    void SetUp() override
    {
        ON_CALL(m_environment_mock, background_scheduler()).WillByDefault(testing::ReturnRef(m_scheduler));
        ON_CALL(m_environment_mock, get_recovery_retry_delay()).WillByDefault(testing::Return(std::chrono::milliseconds{1}));
        ON_CALL(m_environment_mock, get_codec()).WillByDefault(testing::Return(std::make_shared<hareflow::RawCodec>()));
        ON_CALL(m_environment_mock, get_base_client_parameters()).WillByDefault(testing::Return(hareflow::ClientParameters{}));
        ON_CALL(m_environment_mock, get_producer_client(stream_name, testing::_)).WillByDefault(testing::Return(m_internal_client));
        ON_CALL(m_client_mock, max_frame_size()).WillByDefault(testing::Return(1024 * 1024));
    }

    hareflow::ProducerBuilder producer_builder()
    {
        return hareflow::ProducerBuilder{m_internal_environment}
            .stream(std::string{stream_name})
            .batch_publishing_delay(std::chrono::seconds::zero())
            .confirm_timeout(std::chrono::seconds::zero())
            .enqueue_timeout(std::chrono::seconds::zero());
    }

    hareflow::detail::BackgroundScheduler                       m_scheduler;
    testing::NiceMock<hareflow::tests::InternalEnvironmentMock> m_environment_mock;
    testing::NiceMock<hareflow::tests::InternalClientMock>      m_client_mock;
    hareflow::detail::InternalEnvironmentPtr                    m_internal_environment;
    hareflow::detail::InternalClientPtr                         m_internal_client;
};

TEST_F(ProducerTest, BasicStartStop)
{
    testing::InSequence s;

    EXPECT_CALL(m_environment_mock, register_producer(testing::_));
    EXPECT_CALL(m_client_mock, start());
    EXPECT_CALL(m_client_mock, declare_publisher(testing::_, std::string_view{}, stream_name));
    EXPECT_CALL(m_client_mock, delete_publisher(testing::_));
    EXPECT_CALL(m_client_mock, stop());
    EXPECT_CALL(m_environment_mock, unregister_producer(testing::_));

    hareflow::ProducerPtr producer = producer_builder().build();
}

TEST_F(ProducerTest, NamedPublisher)
{
    EXPECT_CALL(m_client_mock, declare_publisher(testing::_, publisher_name, stream_name));
    EXPECT_CALL(m_client_mock, delete_publisher(testing::_));
    EXPECT_CALL(m_client_mock, query_publisher_sequence(publisher_name, stream_name)).WillOnce(testing::Return(123));
    EXPECT_CALL(m_client_mock,
                publish_encoded(
                    testing::_,
                    testing::AllOf(testing::SizeIs(1), testing::Contains(testing::Field(&hareflow::detail::PublishCommand::Message::m_publishing_id, 124)))));

    hareflow::ProducerPtr producer = producer_builder().name(std::string{publisher_name}).build();
    producer->send(hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {});
    producer->flush();
}

TEST_F(ProducerTest, PeriodicFlush)
{
    std::promise<void> published;
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::SizeIs(1))).WillOnce(testing::Invoke([&]() { published.set_value(); }));

    hareflow::ProducerPtr producer = producer_builder().batch_size(5).batch_publishing_delay(std::chrono::milliseconds{1}).build();
    producer->send(hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {});

    EXPECT_EQ(published.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
}

TEST_F(ProducerTest, BatchFullFlush)
{
    std::promise<void> published;
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::SizeIs(5))).WillOnce(testing::Invoke([&]() { published.set_value(); }));

    hareflow::ProducerPtr producer = producer_builder().batch_size(5).build();
    for (int i = 0; i < 5; ++i) {
        producer->send(hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {});
    }

    EXPECT_EQ(published.get_future().wait_for(std::chrono::seconds::zero()), std::future_status::ready);
}

TEST_F(ProducerTest, ExplicitFlush)
{
    std::promise<void> published;
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::SizeIs(1))).WillOnce(testing::Invoke([&]() { published.set_value(); }));

    hareflow::ProducerPtr producer = producer_builder().batch_size(5).build();
    producer->send(hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {});
    producer->flush();

    EXPECT_EQ(published.get_future().wait_for(std::chrono::seconds::zero()), std::future_status::ready);
}

TEST_F(ProducerTest, ExplicitPublishingId)
{
    std::vector<hareflow::detail::PublishCommand::Message> messages;
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::_)).WillOnce(testing::SaveArg<1>(&messages));

    hareflow::ProducerPtr producer = producer_builder().build();
    producer->send(123, hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {});
    producer->flush();

    ASSERT_THAT(messages,
                testing::AllOf(testing::SizeIs(1), testing::Contains(testing::Field(&hareflow::detail::PublishCommand::Message::m_publishing_id, 123))));
}

TEST_F(ProducerTest, EnqueueTimeout)
{
    hareflow::ProducerPtr producer = producer_builder().max_unconfirmed(1).enqueue_timeout(std::chrono::milliseconds{1}).build();
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::SizeIs(1)));

    producer->send(hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {});
    producer->flush();

    EXPECT_THAT([&]() { producer->send(hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {}); },
                testing::Throws<hareflow::ProducerException>(
                    testing::Property(&hareflow::ProducerException::code, hareflow::ProducerErrorCode::MessageEnqueuingFailed)));
}

TEST_F(ProducerTest, ConfirmTimeout)
{
    hareflow::ProducerPtr producer = producer_builder().confirm_timeout(std::chrono::milliseconds{1}).build();
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::SizeIs(1)));

    std::promise<void> invoked;
    producer->send(hareflow::MessageBuilder{}.body("hello").build(), [&](auto& confirmation_status) {
        EXPECT_EQ(confirmation_status.confirmed, false);
        EXPECT_EQ(confirmation_status.status_code, static_cast<std::uint16_t>(hareflow::ProducerErrorCode::ConfirmTimeout));
        invoked.set_value();
    });
    producer->flush();

    EXPECT_EQ(invoked.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
}

TEST_F(ProducerTest, StopFailsUnconfirmed)
{
    hareflow::ProducerPtr producer = producer_builder().build();
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::SizeIs(1)));

    bool invoked = false;
    producer->send(hareflow::MessageBuilder{}.body("hello").build(), [&](auto& confirmation_status) {
        EXPECT_EQ(confirmation_status.confirmed, false);
        EXPECT_EQ(confirmation_status.status_code, static_cast<std::uint16_t>(hareflow::ProducerErrorCode::ProducerStopped));
        invoked = true;
    });
    producer->flush();
    producer = nullptr;

    EXPECT_EQ(invoked, true);
}

TEST_F(ProducerTest, ConfirmPropagation)
{
    std::vector<hareflow::detail::PublishCommand::Message> messages;
    hareflow::ClientParameters                             client_parameters;

    EXPECT_CALL(m_environment_mock, get_producer_client(testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)));
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::_)).WillOnce(testing::SaveArg<1>(&messages));

    hareflow::ProducerPtr producer = producer_builder().build();

    int confirmed = 0;
    for (int i = 0; i < 3; ++i) {
        producer->send(hareflow::MessageBuilder{}.body("hello").build(), [&](auto& confirmation_status) {
            EXPECT_EQ(confirmation_status.confirmed, true);
            EXPECT_THAT(messages,
                        testing::Contains(testing::Field(&hareflow::detail::PublishCommand::Message::m_publishing_id, confirmation_status.publishing_id)));
            ++confirmed;
        });
    }
    producer->flush();
    for (const auto& message : messages) {
        client_parameters.get_publish_confirm_listener()(0, message.m_publishing_id);
    }

    EXPECT_EQ(confirmed, 3);
}

TEST_F(ProducerTest, ConfirmErrorPropagation)
{
    std::vector<hareflow::detail::PublishCommand::Message> messages;
    hareflow::ClientParameters                             client_parameters;

    EXPECT_CALL(m_environment_mock, get_producer_client(testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)));
    EXPECT_CALL(m_client_mock, publish_encoded(testing::_, testing::_)).WillOnce(testing::SaveArg<1>(&messages));

    hareflow::ProducerPtr producer = producer_builder().build();

    int errors = 0;
    for (int i = 0; i < 3; ++i) {
        producer->send(hareflow::MessageBuilder{}.body("hello").build(), [&](auto& confirmation_status) {
            EXPECT_EQ(confirmation_status.confirmed, false);
            EXPECT_EQ(confirmation_status.status_code, static_cast<std::uint16_t>(hareflow::ResponseCode::StreamDoesNotExist));
            EXPECT_THAT(messages,
                        testing::Contains(testing::Field(&hareflow::detail::PublishCommand::Message::m_publishing_id, confirmation_status.publishing_id)));
            ++errors;
        });
    }
    producer->flush();
    for (const auto& message : messages) {
        client_parameters.get_publish_error_listener()(0, message.m_publishing_id, hareflow::ResponseCode::StreamDoesNotExist);
    }

    EXPECT_EQ(errors, 3);
}

TEST_F(ProducerTest, ReconnectRetries)
{
    hareflow::ClientParameters client_parameters;
    std::promise<void>         done;

    EXPECT_CALL(m_environment_mock, get_producer_client(testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)))
        .WillRepeatedly(testing::Return(m_internal_client));
    EXPECT_CALL(m_client_mock, start())
        .WillOnce(testing::Return())
        .WillOnce(testing::Throw(hareflow::IOException("remote host not available")))
        .WillOnce(testing::Return());
    EXPECT_CALL(m_client_mock, declare_publisher(testing::_, std::string_view{}, stream_name)).WillOnce(testing::Return()).WillOnce(testing::Invoke([&]() {
        done.set_value();
    }));

    hareflow::ProducerPtr producer = producer_builder().build();

    client_parameters.get_shutdown_listener()(hareflow::ShutdownReason::Unknown);

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
}

TEST_F(ProducerTest, ReconnectResends)
{
    hareflow::ClientParameters client_parameters;
    std::promise<void>         done;

    EXPECT_CALL(m_environment_mock, get_producer_client(testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)))
        .WillRepeatedly(testing::Return(m_internal_client));
    EXPECT_CALL(
        m_client_mock,
        publish_encoded(testing::_,
                        testing::AllOf(testing::SizeIs(1), testing::Contains(testing::Field(&hareflow::detail::PublishCommand::Message::m_publishing_id, 0)))))
        .Times(2)
        .WillOnce(testing::Return())
        .WillOnce(testing::Invoke([&]() { done.set_value(); }));

    hareflow::ProducerPtr producer = producer_builder().build();
    producer->send(0, hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {});
    producer->flush();

    client_parameters.get_shutdown_listener()(hareflow::ShutdownReason::Unknown);

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
}

TEST_F(ProducerTest, StreamDeleted)
{
    hareflow::ClientParameters client_parameters;
    std::promise<void>         done;
    EXPECT_CALL(m_environment_mock, get_producer_client(testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&client_parameters), testing::Return(m_internal_client)))
        .WillOnce(testing::Throw(hareflow::ResponseErrorException(hareflow::ResponseCode::StreamDoesNotExist)));

    hareflow::ProducerPtr producer = producer_builder().build();
    producer->send(hareflow::MessageBuilder{}.body("hello").build(), [&](auto& confirmation_status) {
        EXPECT_EQ(confirmation_status.confirmed, false);
        EXPECT_EQ(confirmation_status.status_code, static_cast<std::uint16_t>(hareflow::ResponseCode::StreamDoesNotExist));
        done.set_value();
    });

    client_parameters.get_metadata_listener()(stream_name, hareflow::ResponseCode::StreamNotAvailable);

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{1}), std::future_status::ready);
    EXPECT_THAT(
        [&]() { producer->send(hareflow::MessageBuilder{}.body("hello").build(), [](auto...) {}); },
        testing::Throws<hareflow::ProducerException>(testing::Property(&hareflow::ProducerException::code, hareflow::ProducerErrorCode::ProducerStopped)));
}