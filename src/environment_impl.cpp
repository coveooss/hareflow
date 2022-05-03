#include "hareflow/detail/environment_impl.h"

#include <random>

#include "hareflow/logging.h"
#include "hareflow/detail/client_impl.h"
#include "hareflow/detail/consumer_impl.h"
#include "hareflow/detail/producer_impl.h"

namespace {

template<typename Container> auto select_randomly(Container& candidates)
{
    if (candidates.size() > 1) {
        static thread_local std::mt19937           generator{std::random_device{}()};
        std::uniform_int_distribution<std::size_t> distribution{0, candidates.size() - 1};
        return *std::next(candidates.begin(), distribution(generator));
    } else {
        return *candidates.begin();
    }
}

}  // namespace

namespace hareflow::detail {

std::shared_ptr<EnvironmentImpl> EnvironmentImpl::create(std::vector<std::string>  hosts,
                                                         std::chrono::milliseconds recovery_retry_delay,
                                                         ClientParameters          client_parameters)
{
    return std::shared_ptr<EnvironmentImpl>{new EnvironmentImpl{std::move(hosts), recovery_retry_delay, std::move(client_parameters)}};
}

EnvironmentImpl::~EnvironmentImpl()
{
    Logger::debug("Destroying environment");
    m_reconnect_locator_task.destroy();
    if (m_locator != nullptr) {
        m_locator->stop();
    }
    for (auto& weak_producer : m_producers) {
        if (auto producer = weak_producer.lock()) {
            producer->stop();
        }
    }
    for (auto& weak_consumer : m_consumers) {
        if (auto consumer = weak_consumer.lock()) {
            consumer->stop();
        }
    }
}

ProducerBuilder EnvironmentImpl::producer_builder()
{
    return ProducerBuilder{shared_from_this()};
}

ConsumerBuilder EnvironmentImpl::consumer_builder()
{
    return ConsumerBuilder{shared_from_this()};
}

StreamCreator EnvironmentImpl::stream_creator()
{
    return StreamCreator{shared_from_this()};
}

void EnvironmentImpl::delete_stream(std::string_view stream)
{
    locator_operation([&stream](Client& client) { client.delete_stream(stream); });
}

void EnvironmentImpl::register_producer(InternalProducerWeakPtr producer)
{
    std::unique_lock lock{m_mutex};
    m_producers.emplace(std::move(producer));
}

InternalClientPtr EnvironmentImpl::get_producer_client(std::string_view stream, ClientParameters parameters)
{
    std::string host;
    locator_operation([&host, &stream](Client& client) {
        StreamMetadata metadata = fetch_stream_metadata(client, stream);
        host                    = std::move(metadata.get_leader().get_host());
    });
    return ClientImpl::create(std::move(parameters.with_host(std::move(host))));
}

void EnvironmentImpl::unregister_producer(InternalProducerWeakPtr producer)
{
    std::unique_lock lock{m_mutex};
    m_producers.erase(producer);
}

void EnvironmentImpl::register_consumer(InternalConsumerWeakPtr consumer)
{
    std::unique_lock lock{m_mutex};
    m_consumers.emplace(std::move(consumer));
}

InternalClientPtr EnvironmentImpl::get_consumer_client(std::string_view stream, ClientParameters parameters)
{
    std::string host;
    locator_operation([&host, &stream](Client& client) {
        StreamMetadata metadata = fetch_stream_metadata(client, stream);
        if (!metadata.get_replicas().empty()) {
            host = std::move(select_randomly(metadata.get_replicas()).get_host());
        } else {
            host = std::move(metadata.get_leader().get_host());
        }
    });
    return ClientImpl::create(std::move(parameters.with_host(std::move(host))));
}

void EnvironmentImpl::unregister_consumer(InternalConsumerWeakPtr consumer)
{
    std::unique_lock lock{m_mutex};
    m_consumers.erase(consumer);
}

std::chrono::milliseconds EnvironmentImpl::get_recovery_retry_delay()
{
    return m_recovery_retry_delay;
}

ClientParameters EnvironmentImpl::get_base_client_parameters()
{
    return m_client_parameters;
}

CodecPtr EnvironmentImpl::get_codec()
{
    return m_client_parameters.get_codec();
}

BackgroundScheduler& EnvironmentImpl::background_scheduler()
{
    return m_scheduler;
}

void EnvironmentImpl::locator_operation(std::function<void(Client&)> operation)
{
    static constexpr int NB_RETRIES = 3;
    for (int i = 1;; ++i) {
        InternalClientPtr client;
        {
            std::unique_lock lock{m_mutex};
            if (!m_locator_reconnected.wait_for(lock, m_recovery_retry_delay, [this]() { return m_locator != nullptr; })) {
                if (i >= NB_RETRIES) {
                    throw StreamException("Locator not available");
                }
                Logger::debug("Locator operation failed ({} retries left): Locator not available", NB_RETRIES - i);
                continue;
            }
            client = m_locator;
        }

        try {
            operation(*client);
            return;
        } catch (const TimeoutException& e) {
            if (i >= NB_RETRIES) {
                throw;
            }
            Logger::debug("Locator operation failed ({} retries left): {}", NB_RETRIES - i, e.what());
        } catch (const IOException& e) {
            if (i >= NB_RETRIES) {
                throw;
            }
            Logger::debug("Locator operation failed ({} retries left): {}", NB_RETRIES - i, e.what());
        }
    }
}

StreamMetadata EnvironmentImpl::fetch_stream_metadata(Client& client, std::string_view stream)
{
    StreamMetadataSet metadata_set = client.metadata({std::string{stream}});
    if (metadata_set.empty()) {
        // Supposed to be impossible
        throw ResponseErrorException{ResponseCode::StreamDoesNotExist};
    }
    StreamMetadata metadata = std::move(metadata_set.extract(metadata_set.begin()).value());
    if (metadata.get_response_code() != ResponseCode::Ok) {
        throw ResponseErrorException{metadata.get_response_code()};
    }
    return metadata;
}

EnvironmentImpl::EnvironmentImpl(std::vector<std::string> hosts, std::chrono::milliseconds recovery_retry_delay, ClientParameters client_parameters)
 : m_hosts{std::move(hosts)},
   m_recovery_retry_delay{recovery_retry_delay},
   m_client_parameters{std::move(client_parameters)},
   m_scheduler{},
   m_reconnect_locator_task{},
   m_mutex{},
   m_locator{},
   m_producers{},
   m_consumers{}
{
    m_reconnect_locator_task = ReschedulableTask(m_scheduler, [this]() { reconnect_to_server(); });
    m_locator                = get_locator_client();
    m_locator->start();
}

InternalClientPtr EnvironmentImpl::get_locator_client()
{
    namespace ph = std::placeholders;
    return ClientImpl::create(get_base_client_parameters()
                                  .with_host(select_randomly(m_hosts))
                                  .with_shutdown_listener(std::bind(&EnvironmentImpl::handle_locator_shutdown, this, ph::_1)));
}

void EnvironmentImpl::handle_locator_shutdown(ShutdownReason reason)
{
    if (reason != ShutdownReason::ClientInitiated) {
        Logger::info("Triggering locator recovery because of unexpected client shutdown");
        std::unique_lock lock{m_mutex};
        m_locator = nullptr;
        m_reconnect_locator_task.schedule_now();
    }
}

void EnvironmentImpl::reconnect_to_server()
{
    {
        std::unique_lock lock{m_mutex};
        if (m_locator != nullptr) {
            return;
        }
    }

    InternalClientPtr client;
    try {
        client = get_locator_client();
        client->start();
        {
            std::unique_lock lock{m_mutex};
            m_locator = client;
        }
        m_locator_reconnected.notify_all();
        Logger::info("Locator recovery successful");
    } catch (const StreamException& e) {
        Logger::warn("Locator recovery failed, will retry: {}", e.what());
        if (client != nullptr) {
            client->stop();
        }
        m_reconnect_locator_task.schedule_after(m_recovery_retry_delay);
    }
}

}  // namespace hareflow::detail