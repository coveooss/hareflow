#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "hareflow/consumer.h"
#include "hareflow/detail/internal_types.h"
#include "hareflow/detail/reschedulable_task.h"

namespace hareflow::detail {

class InternalConsumer : public Consumer
{
public:
    virtual ~InternalConsumer() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;
};

class ConsumerImpl final : public InternalConsumer, public std::enable_shared_from_this<ConsumerImpl>
{
public:
    static std::shared_ptr<ConsumerImpl> create(std::string                                 name,
                                                std::string                                 stream,
                                                OffsetSpecification                         offset_specification,
                                                std::optional<AutomaticCursorConfiguration> cursor_configuration,
                                                bool                                        automatic_credits,
                                                MessageHandler                              message_handler,
                                                InternalEnvironmentPtr                      environment);
    ~ConsumerImpl();

    void start() override;
    void stop() override;

    void store_offset(std::uint64_t offset) override;
    void grant_credit() override;

private:
    enum class Status { Initialized, Starting, Started, Stopped };

    ConsumerImpl(std::string                                 name,
                 std::string                                 stream,
                 OffsetSpecification                         offset_specification,
                 std::optional<AutomaticCursorConfiguration> cursor_configuration,
                 bool                                        automatic_credits,
                 MessageHandler                              message_handler,
                 InternalEnvironmentPtr                      environment);

    InternalEnvironmentPtr get_environment();
    InternalClientPtr      get_client(InternalEnvironmentPtr environment);
    void                   internal_stop();
    void                   internal_store_offset(std::uint64_t offset);
    void                   handle_message(std::int64_t timestamp, std::uint64_t offset, MessagePtr message);
    void                   handle_metadata_update(std::string_view stream);
    void                   handle_shutdown(ShutdownReason reason);
    void                   store_current_offset();
    void                   delayed_reconnect();
    void                   reconnect_to_server();

    const std::string                                 m_name;
    const std::string                                 m_stream;
    const std::optional<AutomaticCursorConfiguration> m_auto_cursor_config;
    const bool                                        m_automatic_credits;
    std::chrono::milliseconds                         m_recovery_retry_delay;
    const InternalEnvironmentWeakPtr                  m_environment;
    MessageHandler                                    m_message_handler;
    std::atomic<Status>                               m_status;
    std::mutex                                        m_mutex;
    std::uint8_t                                      m_subscription_id;
    std::optional<OffsetSpecification>                m_initial_offset;
    std::optional<std::uint64_t>                      m_last_seen_offset;
    std::uint32_t                                     m_messages_since_last_persist;
    InternalClientPtr                                 m_client;
    ReschedulableTask                                 m_persist_cursor_task;
    ReschedulableTask                                 m_reconnect_task;
    ReschedulableTask                                 m_delayed_reconnect_task;
    ReschedulableTask                                 m_stream_deleted_task;
};

}  // namespace hareflow::detail