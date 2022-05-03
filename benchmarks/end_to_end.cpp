#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <random>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "hareflow.h"

static constexpr std::uint32_t publish_count = 1'000'000;

std::string random_string()
{
    static constexpr int                                           length{16};
    static constexpr std::string_view                              chars{"abcdefghijklmnopqrstuvwxyz"};
    static thread_local std::mt19937                               generator{std::random_device{}()};
    static thread_local std::uniform_int_distribution<std::size_t> distribution{0, chars.size() - 1};

    std::string result;
    result.resize(length);
    for (int i = 0; i < length; ++i) {
        result[i] = chars[distribution(generator)];
    }
    return result;
}

class TemporaryStream
{
public:
    TemporaryStream(hareflow::EnvironmentPtr environment) : m_environment(environment), m_stream(random_string())
    {
        m_environment->stream_creator().stream(m_stream).create();
    }

    ~TemporaryStream()
    {
        try {
            m_environment->delete_stream(m_stream);
        } catch (...) {
        }
    }

    const std::string& name()
    {
        return m_stream;
    }

private:
    hareflow::EnvironmentPtr m_environment;
    std::string              m_stream;
};

void publish_messages(hareflow::EnvironmentPtr environment, const std::string& stream)
{
    std::mutex              mutex;
    std::condition_variable done;
    std::atomic<uint32_t>   confirm_count = 0;
    hareflow::ProducerPtr   producer      = environment->producer_builder().stream(stream).build();
    for (std::uint32_t i = 0; i < publish_count; ++i) {
        hareflow::MessagePtr message = hareflow::MessageBuilder().body(std::to_string(i)).build();
        producer->send(message, [&](const hareflow::ConfirmationStatus& confirmation) {
            EXPECT_EQ(confirmation.confirmed, true);
            if (++confirm_count == publish_count) {
                {
                    std::unique_lock lock{mutex};
                }
                done.notify_all();
            }
        });
    }
    std::unique_lock lock{mutex};
    auto             successful = done.wait_for(lock, std::chrono::seconds(60), [&]() { return confirm_count == publish_count; });
    EXPECT_EQ(successful, true);
}

void consume_messages(hareflow::EnvironmentPtr environment, const std::string& stream)
{
    std::mutex              mutex;
    std::condition_variable done;
    std::atomic<uint32_t>   consume_count = 0;
    hareflow::ConsumerPtr   consumer      = environment->consumer_builder()
                                         .offset_specification(hareflow::OffsetSpecification::first())
                                         .stream(stream)
                                         .message_handler([&](auto...) {
                                             if (++consume_count == publish_count) {
                                                 {
                                                     std::unique_lock lock{mutex};
                                                 }
                                                 done.notify_all();
                                             }
                                         })
                                         .build();

    std::unique_lock lock{mutex};
    bool             successful = done.wait_for(lock, std::chrono::seconds(60), [&]() { return consume_count == publish_count; });
    EXPECT_EQ(successful, true);
}

void run_producer_benchmark(hareflow::EnvironmentPtr environment)
{
    TemporaryStream stream{environment};

    fmt::print("Starting message publishing\n");
    auto start = std::chrono::steady_clock::now();

    publish_messages(environment, stream.name());

    std::chrono::duration<double> duration{std::chrono::steady_clock::now() - start};
    fmt::print("Done publishing {} messages after {}s\n", publish_count, duration.count());
}

void run_consumer_benchmark(hareflow::EnvironmentPtr environment)
{
    TemporaryStream stream{environment};

    fmt::print("Preparing messages...\n");
    publish_messages(environment, stream.name());

    fmt::print("Starting message consuming\n");
    auto start = std::chrono::steady_clock::now();

    consume_messages(environment, stream.name());

    std::chrono::duration<double> duration{std::chrono::steady_clock::now() - start};
    fmt::print("Done consuming {} messages after {}s\n", publish_count, duration.count());
}

TEST(ProducerBench, ProtonCodec)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().codec(std::make_shared<hareflow::QpidProtonCodec>()).build();
    run_producer_benchmark(environment);
}

TEST(ProducerBench, RawCodec)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().codec(std::make_shared<hareflow::RawCodec>()).build();
    run_producer_benchmark(environment);
}

TEST(ConsumerBench, ProtonCodec)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().codec(std::make_shared<hareflow::QpidProtonCodec>()).build();
    run_consumer_benchmark(environment);
}

TEST(ConsumerBench, RawCodec)
{
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().codec(std::make_shared<hareflow::RawCodec>()).build();
    run_consumer_benchmark(environment);
}