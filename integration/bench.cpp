#include <atomic>
#include <chrono>
#include <future>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "hareflow.h"
#include "utils.h"

namespace hareflow::tests {

void publish_messages(EnvironmentPtr environment, const std::string& stream, std::uint32_t count)
{
    std::promise<void>    done;
    std::atomic<uint32_t> confirm_count = 0;
    ProducerPtr           producer      = environment->producer_builder().stream(stream).build();
    for (std::uint32_t i = 0; i < count; ++i) {
        MessagePtr message = MessageBuilder().body(std::to_string(i)).build();
        producer->send(message, [&](const ConfirmationStatus& confirmation) {
            EXPECT_EQ(confirmation.confirmed, true);
            if (++confirm_count == count) {
                done.set_value();
            }
        });
    }
    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{60}), std::future_status::ready);
}

void consume_messages(EnvironmentPtr environment, const std::string& stream, std::uint32_t count)
{
    std::promise<void>    done;
    std::atomic<uint32_t> consume_count = 0;
    ConsumerPtr           consumer      = environment->consumer_builder()
                               .offset_specification(OffsetSpecification::first())
                               .stream(stream)
                               .message_handler([&](auto...) {
                                   if (++consume_count == count) {
                                       done.set_value();
                                   }
                               })
                               .build();

    EXPECT_EQ(done.get_future().wait_for(std::chrono::seconds{60}), std::future_status::ready);
}

void run_producer_benchmark(EnvironmentPtr environment, std::uint32_t count)
{
    TemporaryStream stream{environment};

    fmt::print("Starting message publishing\n");
    auto start = std::chrono::steady_clock::now();

    publish_messages(environment, stream.name(), count);

    std::chrono::duration<double> duration{std::chrono::steady_clock::now() - start};
    fmt::print("Done publishing {} messages after {}s\n", count, duration.count());
}

void run_consumer_benchmark(EnvironmentPtr environment, std::uint32_t count)
{
    TemporaryStream stream{environment};

    fmt::print("Preparing messages...\n");
    publish_messages(environment, stream.name(), count);

    fmt::print("Starting message consuming\n");
    auto start = std::chrono::steady_clock::now();

    consume_messages(environment, stream.name(), count);

    std::chrono::duration<double> duration{std::chrono::steady_clock::now() - start};
    fmt::print("Done consuming {} messages after {}s\n", count, duration.count());
}

}  // namespace hareflow::tests

static constexpr std::uint32_t publish_count = 1'000'000;

TEST(ProducerBench, ProtonCodec)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().codec(std::make_shared<hareflow::QpidProtonCodec>()).build();
    hareflow::tests::run_producer_benchmark(environment, publish_count);
}

TEST(ProducerBench, RawCodec)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().codec(std::make_shared<hareflow::RawCodec>()).build();
    hareflow::tests::run_producer_benchmark(environment, publish_count);
}

TEST(ConsumerBench, ProtonCodec)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().codec(std::make_shared<hareflow::QpidProtonCodec>()).build();
    hareflow::tests::run_consumer_benchmark(environment, publish_count);
}

TEST(ConsumerBench, RawCodec)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().codec(std::make_shared<hareflow::RawCodec>()).build();
    hareflow::tests::run_consumer_benchmark(environment, publish_count);
}