#include <atomic>
#include <chrono>
#include <future>

#include <gtest/gtest.h>

#include "hareflow.h"
#include "utils.h"

namespace {

void publish(const hareflow::EnvironmentPtr& environment, const std::string& stream, std::string_view body)
{
    std::promise<void> confirmed;

    hareflow::ProducerPtr producer = environment->producer_builder().stream(stream).build();
    producer->send(hareflow::MessageBuilder().body(body).build(), [&](auto& status) {
        EXPECT_EQ(status.confirmed, true);
        confirmed.set_value();
    });
    ASSERT_EQ(confirmed.get_future().wait_for(std::chrono::seconds{5}), std::future_status::ready);
}

}  // namespace

TEST(ConsumerIntegrationTest, ConsumeFromFirst)
{
    hareflow::EnvironmentPtr         environment = hareflow::EnvironmentBuilder().build();
    hareflow::tests::TemporaryStream stream{environment};

    publish(environment, stream.name(), "hello");

    std::promise<hareflow::MessagePtr> received;
    std::atomic_bool                   delivered{false};
    hareflow::ConsumerPtr              consumer = environment->consumer_builder()
                                         .stream(stream.name())
                                         .offset_specification(hareflow::OffsetSpecification::first())
                                         .message_handler([&](auto& /*context*/, auto message) {
                                             if (!delivered.exchange(true)) {
                                                 received.set_value(std::move(message));
                                             }
                                         })
                                         .build();

    std::future<hareflow::MessagePtr> future = received.get_future();
    ASSERT_EQ(future.wait_for(std::chrono::seconds{5}), std::future_status::ready);
    hareflow::MessagePtr message = future.get();
    EXPECT_EQ(std::string(message->body().begin(), message->body().end()), "hello");
}

TEST(ConsumerIntegrationTest, CommittedChunkIdIsDelivered)
{
    hareflow::EnvironmentPtr         environment = hareflow::EnvironmentBuilder().build();
    hareflow::tests::TemporaryStream stream{environment};

    publish(environment, stream.name(), "hello");

    std::promise<hareflow::MessageContext> received;
    std::atomic_bool                       delivered{false};
    hareflow::ConsumerPtr                  consumer = environment->consumer_builder()
                                         .stream(stream.name())
                                         .offset_specification(hareflow::OffsetSpecification::first())
                                         .message_handler([&](auto& context, const auto& /*message*/) {
                                             if (!delivered.exchange(true)) {
                                                 received.set_value(context);
                                             }
                                         })
                                         .build();

    std::future<hareflow::MessageContext> future = received.get_future();
    ASSERT_EQ(future.wait_for(std::chrono::seconds{5}), std::future_status::ready);

    EXPECT_TRUE(future.get().committed_chunk_id.has_value());
}
