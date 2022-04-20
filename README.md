# Hareflow
Hareflow is a RabbitMQ stream client for C++.

It uses an API similar to the existing official Java client, with the ability to create, delete, consume from, and publish to streams.

![License](https://img.shields.io/github/license/coveooss/hareflow)

## Project State and Maturity
The project is still in development, however most APIs are expected to remain stable and API breaking changes will be kept to a minimum.

A low-level client is also exposed, though its direct usage is generally not needed and its API is subject to breaking changes.

No guarantees are made about ABI stability.

## Getting Started
```cpp
#include <chrono>

#include <hareflow/hareflow.h>

int main()
{
    // Note: these are the default values, they are only specified for illustration purposes.
    hareflow::EnvironmentPtr environment = hareflow::EnvironmentBuilder().host("localhost").username("guest").password("guest").build();

    environment->stream_creator().stream("my-stream").max_age(std::chrono::hours(6)).create();

    int nb_confirmed = 0;
    hareflow::ProducerPtr producer = environment->producer_builder().stream("my-stream").build();
    for (int i = 0; i < 5; ++i) {
        hareflow::MessagePtr message = hareflow::MessageBuilder().body("some data").build();
        producer->send(message, [&](const hareflow::ConfirmationStatus& confirmation_status) {
            if (status.confirmed) {
                ++nb_confirmed;
            } else {
                // deal with error appropriately
            }
        });
    }
    // Note: waiting for confirmations omitted for simplicity
    producer = nullptr;

    int nb_consumed = 0;
    auto message_handler = [&](const hareflow::MessageContext& context, hareflow::MessagePtr message) {
        ++nb_consumed;
    };
    hareflow::ConsumerPtr consumer = environment->consumer_builder().offset(hareflow::OffsetSpecification::first()).stream("my-stream").message_handler(message_handler).build();
    // Note: waiting for messages omitted for simplicity
    consumer = nullptr;

    return 0;
}
```

## Installation
Hareflow is integrated to vcpkg and it is highly suggested to use it to build and install the library: https://vcpkg.info/port/hareflow

Hareflow supports GCC, Clang, and MSVC, and requires a compiler with support for C++17 features.