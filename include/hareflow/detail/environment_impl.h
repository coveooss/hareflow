#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <set>

#include "hareflow/client_parameters.h"
#include "hareflow/environment.h"
#include "hareflow/detail/background_scheduler.h"
#include "hareflow/detail/client_impl.h"
#include "hareflow/detail/internal_types.h"
#include "hareflow/detail/reschedulable_task.h"

namespace hareflow::detail {

class HAREFLOW_EXPORT InternalEnvironment : public Environment
{
public:
    virtual ~InternalEnvironment() = default;

    virtual void              register_producer(InternalProducerWeakPtr producer)                       = 0;
    virtual InternalClientPtr get_producer_client(std::string_view stream, ClientParameters parameters) = 0;
    virtual void              unregister_producer(InternalProducerWeakPtr producer)                     = 0;

    virtual void              register_consumer(InternalConsumerWeakPtr consumer)                       = 0;
    virtual InternalClientPtr get_consumer_client(std::string_view stream, ClientParameters parameters) = 0;
    virtual void              unregister_consumer(InternalConsumerWeakPtr consumer)                     = 0;

    virtual std::chrono::milliseconds get_recovery_retry_delay()                                = 0;
    virtual ClientParameters          get_base_client_parameters()                              = 0;
    virtual CodecPtr                  get_codec()                                               = 0;
    virtual BackgroundScheduler&      background_scheduler()                                    = 0;
    virtual void                      locator_operation(std::function<void(Client&)> operation) = 0;
};

class EnvironmentImpl final : public InternalEnvironment, public std::enable_shared_from_this<EnvironmentImpl>
{
public:
    static std::shared_ptr<EnvironmentImpl> create(std::vector<std::string>  hosts,
                                                   std::chrono::milliseconds recovery_retry_delay,
                                                   ClientParameters          client_parameters);
    virtual ~EnvironmentImpl();

    ProducerBuilder producer_builder() override;
    ConsumerBuilder consumer_builder() override;
    StreamCreator   stream_creator() override;
    void            delete_stream(std::string_view stream) override;

    void              register_producer(InternalProducerWeakPtr producer) override;
    InternalClientPtr get_producer_client(std::string_view stream, ClientParameters parameters) override;
    void              unregister_producer(InternalProducerWeakPtr producer) override;

    void              register_consumer(InternalConsumerWeakPtr consumer) override;
    InternalClientPtr get_consumer_client(std::string_view stream, ClientParameters parameters) override;
    void              unregister_consumer(InternalConsumerWeakPtr consumer) override;

    std::chrono::milliseconds get_recovery_retry_delay() override;
    ClientParameters          get_base_client_parameters() override;
    CodecPtr                  get_codec() override;
    BackgroundScheduler&      background_scheduler() override;
    void                      locator_operation(std::function<void(Client&)> operation) override;

private:
    static StreamMetadata fetch_stream_metadata(Client& client, std::string_view stream);

    EnvironmentImpl(std::vector<std::string> hosts, std::chrono::milliseconds recovery_retry_delay, ClientParameters client_parameters);

    InternalClientPtr get_locator_client();
    void              handle_locator_shutdown(ShutdownReason reason);
    void              reconnect_to_server();

    const std::vector<std::string>                       m_hosts;
    const std::chrono::milliseconds                      m_recovery_retry_delay;
    const ClientParameters                               m_client_parameters;
    BackgroundScheduler                                  m_scheduler;
    ReschedulableTask                                    m_reconnect_locator_task;
    std::mutex                                           m_mutex;
    std::condition_variable                              m_locator_reconnected;
    InternalClientPtr                                    m_locator;
    std::set<InternalProducerWeakPtr, std::owner_less<>> m_producers;
    std::set<InternalConsumerWeakPtr, std::owner_less<>> m_consumers;
};

}  // namespace hareflow::detail