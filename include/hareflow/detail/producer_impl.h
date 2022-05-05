#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "hareflow/producer.h"
#include "hareflow/detail/internal_types.h"
#include "hareflow/detail/reschedulable_task.h"
#include "hareflow/detail/semaphore.h"

namespace hareflow::detail {

class HAREFLOW_EXPORT InternalProducer : public Producer
{
public:
    virtual ~InternalProducer() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;
};

class ProducerImpl final : public InternalProducer, public std::enable_shared_from_this<ProducerImpl>
{
public:
    static std::shared_ptr<ProducerImpl> create(std::string               name,
                                                std::string               stream,
                                                std::uint32_t             batch_size,
                                                std::chrono::milliseconds batch_publishing_delay,
                                                std::uint32_t             max_unconfirmed,
                                                std::chrono::milliseconds confirm_timeout,
                                                std::chrono::milliseconds enqueue_timeout,
                                                InternalEnvironmentPtr    environment);
    virtual ~ProducerImpl();

    void start() override;
    void stop() override;

    void send(MessagePtr message, ConfirmationHandler confirmation_handler) override;
    void send(std::uint64_t publishing_id, MessagePtr message, ConfirmationHandler confirmation_handler) override;
    void flush() override;

private:
    enum class Status { Initialized, Starting, Started, Stopped };

    ProducerImpl(std::string               name,
                 std::string               stream,
                 std::uint32_t             batch_size,
                 std::chrono::milliseconds batch_publishing_delay,
                 std::uint32_t             max_unconfirmed,
                 std::chrono::milliseconds confirm_timeout,
                 std::chrono::milliseconds enqueue_timeout,
                 InternalEnvironmentPtr    environment);

    InternalEnvironmentPtr get_environment();
    InternalClientPtr      get_client(detail::InternalEnvironmentPtr environment);
    void                   handle_publish_confirm(std::uint64_t publishing_id);
    void                   handle_publish_error(std::uint64_t publishing_id, ResponseCode error_code);
    void                   handle_metadata_update(std::string_view stream);
    void                   handle_shutdown(ShutdownReason reason);
    void                   publish_batch();
    void                   check_confirm_timeout();
    void                   delayed_reconnect();
    void                   reconnect_to_server();
    void                   internal_stop(std::uint16_t code);
    void                   fail_all_unconfirmed(std::uint16_t code);

    const std::string                                        m_name;
    const std::string                                        m_stream;
    const std::uint32_t                                      m_batch_size;
    const std::chrono::milliseconds                          m_batch_publishing_delay;
    const std::chrono::milliseconds                          m_confirm_timeout;
    const std::chrono::milliseconds                          m_enqueue_timeout;
    const std::chrono::milliseconds                          m_recovery_retry_delay;
    const InternalEnvironmentWeakPtr                         m_environment;
    std::atomic<std::uint64_t>                               m_next_publishing_id;
    std::unique_ptr<Accumulator>                             m_accumulator;
    std::atomic<Status>                                      m_status;
    std::mutex                                               m_mutex;
    std::uint8_t                                             m_publisher_id;
    Semaphore                                                m_enqueue_semaphore;
    std::unordered_map<std::uint64_t, AccumulatedMessagePtr> m_unconfirmed;
    InternalClientPtr                                        m_client;
    ReschedulableTask                                        m_batch_publish_task;
    ReschedulableTask                                        m_confirm_timeout_task;
    ReschedulableTask                                        m_reconnect_task;
    ReschedulableTask                                        m_delayed_reconnect_task;
    ReschedulableTask                                        m_stream_deleted_task;
};

}  // namespace hareflow::detail