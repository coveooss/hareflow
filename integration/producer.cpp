#include <chrono>
#include <future>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include "hareflow.h"
#include "utils.h"

TEST(ProducerIntegrationTest, Connect)
{
    hareflow::EnvironmentPtr         environment = hareflow::EnvironmentBuilder().build();
    hareflow::tests::TemporaryStream stream{environment};

    hareflow::ProducerPtr producer = environment->producer_builder().stream(stream.name()).build();
}

TEST(ProducerIntegrationTest, SendMessage)
{
    hareflow::EnvironmentPtr         environment = hareflow::EnvironmentBuilder().build();
    hareflow::tests::TemporaryStream stream{environment};

    hareflow::ProducerPtr producer = environment->producer_builder().stream(stream.name()).build();

    std::promise<void> confirmed;
    producer->send(hareflow::MessageBuilder().body("hello").build(), [&](auto& status) {
        EXPECT_EQ(status.confirmed, true);
        confirmed.set_value();
    });
    EXPECT_EQ(confirmed.get_future().wait_for(std::chrono::seconds{5}), std::future_status::ready);
}

TEST(ProducerIntegrationTest, NamedPublisherSequence)
{
    hareflow::EnvironmentPtr         environment = hareflow::EnvironmentBuilder().build();
    hareflow::tests::TemporaryStream stream{environment};

    hareflow::ProducerBuilder builder = environment->producer_builder().stream(stream.name()).name("my-publisher");
    std::uint64_t             first_id;

    {
        hareflow::ProducerPtr producer = builder.build();

        std::promise<void> confirmed;
        producer->send(hareflow::MessageBuilder().body("hello").build(), [&](auto& status) {
            EXPECT_EQ(status.confirmed, true);
            first_id = status.publishing_id;
            confirmed.set_value();
        });
        EXPECT_EQ(confirmed.get_future().wait_for(std::chrono::seconds{5}), std::future_status::ready);
    }

    {
        hareflow::ProducerPtr producer = builder.build();

        std::promise<void> confirmed;
        producer->send(hareflow::MessageBuilder().body("hello").build(), [&](auto& status) {
            EXPECT_EQ(status.confirmed, true);
            EXPECT_EQ(status.publishing_id, first_id + 1);
            confirmed.set_value();
        });
        EXPECT_EQ(confirmed.get_future().wait_for(std::chrono::seconds{5}), std::future_status::ready);
    }
}

TEST(ProducerIntegrationTest, NonExistentStream)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().build();

    EXPECT_THAT([&]() { environment->producer_builder().stream("i-dont-exist").build(); },
                testing::Throws<hareflow::ResponseErrorException>(
                    testing::Property(&hareflow::ResponseErrorException::code, hareflow::ResponseCode::StreamDoesNotExist)));
}

TEST(ProducerIntegrationTest, StreamDeletedWhileRunning)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().recovery_retry_delay(std::chrono::seconds{1}).build();
    auto                     stream      = std::make_unique<hareflow::tests::TemporaryStream>(environment);

    hareflow::ProducerPtr producer = environment->producer_builder().stream(stream->name()).build();

    stream.reset();

    std::promise<void> confirmed;
    producer->send(hareflow::MessageBuilder().body("hello").build(), [&](auto& status) {
        EXPECT_EQ(status.confirmed, false);
        EXPECT_EQ(status.status_code, static_cast<std::uint16_t>(hareflow::ResponseCode::StreamDoesNotExist));
        confirmed.set_value();
    });
    EXPECT_EQ(confirmed.get_future().wait_for(std::chrono::seconds{5}), std::future_status::ready);
}