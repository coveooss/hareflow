#include "hareflow/detail/consumer_impl.h"

#include "hareflow/client_parameters.h"
#include "hareflow/logging.h"

#include "hareflow/detail/client_impl.h"
#include "hareflow/detail/environment_impl.h"

namespace hareflow::detail {

std::shared_ptr<ConsumerImpl> ConsumerImpl::create(std::string                                 name,
                                                   std::string                                 stream,
                                                   OffsetSpecification                         offset_specification,
                                                   std::optional<AutomaticCursorConfiguration> auto_cursor_config,
                                                   bool                                        automatic_credits,
                                                   MessageHandler                              message_handler,
                                                   InternalEnvironmentPtr                      environment)
{
    return std::shared_ptr<ConsumerImpl>{new ConsumerImpl{std::move(name),
                                                          std::move(stream),
                                                          std::move(offset_specification),
                                                          std::move(auto_cursor_config),
                                                          automatic_credits,
                                                          std::move(message_handler),
                                                          std::move(environment)}};
}

ConsumerImpl::~ConsumerImpl()
{
    stop();
}

void ConsumerImpl::start()
{
    Status expected = Status::Initialized;
    if (!m_status.compare_exchange_strong(expected, Status::Starting)) {
        throw StreamException{"Invalid status for start request"};
    }

    std::unique_lock lock{m_mutex};

    auto environment = get_environment();
    environment->register_consumer(weak_from_this());
    m_client = get_client(environment);

    m_client->start();
    if (!m_name.empty()) {
        std::optional<std::uint64_t> stored_offset = m_client->query_offset(m_name, m_stream);
        if (stored_offset) {
            m_initial_offset = OffsetSpecification::offset(*stored_offset + 1);
        }
    }
    m_client->subscribe(m_subscription_id, m_stream, *m_initial_offset, 10, {});

    if (m_persist_cursor_task.valid()) {
        m_persist_cursor_task.schedule_after(m_auto_cursor_config->force_persist_delay());
    }

    expected = Status::Starting;
    if (!m_status.compare_exchange_strong(expected, Status::Started)) {
        throw StreamException("Consumer was stopped while start was ongoing");
    }
}

void ConsumerImpl::stop()
{
    m_stream_deleted_task.destroy();
    internal_stop();
}

void ConsumerImpl::store_offset(std::uint64_t offset)
{
    if (m_status.load() != Status::Started) {
        throw StreamException{"Consumer is stopped"};
    }

    std::unique_lock lock{m_mutex};
    internal_store_offset(offset);
}

void ConsumerImpl::grant_credit()
{
    if (m_status.load() != Status::Started) {
        throw StreamException{"Consumer is stopped"};
    }

    std::unique_lock lock{m_mutex};
    if (!m_automatic_credits && m_client != nullptr) {
        m_client->credit(m_subscription_id, 1);
    }
}

ConsumerImpl::ConsumerImpl(std::string                                 name,
                           std::string                                 stream,
                           OffsetSpecification                         offset_specification,
                           std::optional<AutomaticCursorConfiguration> auto_cursor_config,
                           bool                                        automatic_credits,
                           MessageHandler                              message_handler,
                           InternalEnvironmentPtr                      environment)
 : m_name{std::move(name)},
   m_stream{std::move(stream)},
   m_auto_cursor_config{!m_name.empty() ? std::move(auto_cursor_config) : std::nullopt},
   m_automatic_credits{automatic_credits},
   m_recovery_retry_delay{environment->get_recovery_retry_delay()},
   m_environment{environment},
   m_message_handler{std::move(message_handler)},
   m_status{Status::Initialized},
   m_mutex{},
   m_subscription_id{0},  // No multiplexing for now
   m_initial_offset{std::move(offset_specification)},
   m_last_seen_offset{std::nullopt},
   m_messages_since_last_persist{0},
   m_client{},
   m_persist_cursor_task{},
   m_reconnect_task{},
   m_delayed_reconnect_task{}
{
    if (m_auto_cursor_config && m_auto_cursor_config->force_persist_delay() != std::chrono::milliseconds::zero()) {
        m_persist_cursor_task = ReschedulableTask{environment->background_scheduler(), [this]() { store_current_offset(); }};
    }
    m_reconnect_task         = ReschedulableTask{environment->background_scheduler(), [this]() { reconnect_to_server(); }};
    m_delayed_reconnect_task = ReschedulableTask{environment->background_scheduler(), [this]() { delayed_reconnect(); }};
    m_stream_deleted_task    = ReschedulableTask{environment->background_scheduler(), [this]() { internal_stop(); }};
}

InternalEnvironmentPtr ConsumerImpl::get_environment()
{
    auto environment = m_environment.lock();
    if (environment == nullptr) {
        throw StreamException{"Invalid state: environment destroyed while consumer is alive"};
    }
    return environment;
}

InternalClientPtr ConsumerImpl::get_client(InternalEnvironmentPtr environment)
{
    namespace ph = std::placeholders;

    ClientParameters parameters = environment->get_base_client_parameters()
                                      .with_message_listener(std::bind(&ConsumerImpl::handle_message, this, ph::_2, ph::_3, ph::_4))
                                      .with_metadata_listener(std::bind(&ConsumerImpl::handle_metadata_update, this, ph::_1))
                                      .with_shutdown_listener(std::bind(&ConsumerImpl::handle_shutdown, this, ph::_1));

    if (m_automatic_credits) {
        parameters.with_chunk_listener(std::bind([this](Client& client) { client.credit(m_subscription_id, 1); }, ph::_1));
    }
    return environment->get_consumer_client(m_stream, std::move(parameters));
}

void ConsumerImpl::internal_stop()
{
    m_persist_cursor_task.destroy();
    m_delayed_reconnect_task.destroy();
    m_reconnect_task.destroy();

    if (m_status.exchange(Status::Stopped) != Status::Stopped) {
        InternalClientPtr client;
        {
            std::unique_lock lock{m_mutex};
            client = std::move(m_client);
        }
        if (client != nullptr) {
            try {
                client->unsubscribe(m_subscription_id);
            } catch (const StreamException& e) {
                Logger::warn("Failed to cleanly unsubscribe from stream while stopping consumer: {}", e.what());
            }
            {
                std::unique_lock lock{m_mutex};
                if (m_auto_cursor_config && m_messages_since_last_persist > 0) {
                    try {
                        client->store_offset(m_name, m_stream, *m_last_seen_offset);
                    } catch (const StreamException& e) {
                        Logger::warn("Failed to store offset while stopping consumer: {}", e.what());
                    }
                }
            }
            client->stop();
        }
        if (auto environment = m_environment.lock()) {
            environment->unregister_consumer(weak_from_this());
        }
    }
}

void ConsumerImpl::internal_store_offset(std::uint64_t offset)
{
    if (!m_name.empty() && m_client != nullptr) {
        try {
            m_client->store_offset(m_name, m_stream, offset);
            m_messages_since_last_persist = 0;
        } catch (const StreamException& e) {
            Logger::warn("Failed to persist cursor: {}", e.what());
        }

        if (m_persist_cursor_task.valid()) {
            m_persist_cursor_task.schedule_after(m_auto_cursor_config->force_persist_delay());
        }
    }
}

void ConsumerImpl::handle_message(std::int64_t timestamp, std::uint64_t offset, MessagePtr message)
{
    bool store_offset = false;
    {
        std::unique_lock lock{m_mutex};
        if (m_initial_offset) {
            if (m_initial_offset->is_offset() && m_initial_offset->get_offset() > offset) {
                return;
            }
            m_initial_offset = std::nullopt;
        }
        m_last_seen_offset = offset;
        ++m_messages_since_last_persist;
        store_offset =
            m_auto_cursor_config && m_auto_cursor_config->persist_frequency() > 0 && m_messages_since_last_persist >= m_auto_cursor_config->persist_frequency();
    }

    if (m_message_handler) {
        m_message_handler(MessageContext{offset, timestamp, this}, std::move(message));
    }

    if (store_offset) {
        std::unique_lock lock{m_mutex};
        if (m_messages_since_last_persist >= m_auto_cursor_config->persist_frequency()) {
            internal_store_offset(offset);
        }
    }
}

void ConsumerImpl::handle_metadata_update(std::string_view stream)
{
    if (stream == m_stream) {
        Logger::info("Triggering delayed consumer recovery because of stream metadata update");
        m_delayed_reconnect_task.schedule_now();
    }
}

void ConsumerImpl::handle_shutdown(ShutdownReason reason)
{
    if (reason != ShutdownReason::ClientInitiated) {
        Logger::info("Triggering consumer recovery because of unexpected client shutdown");
        std::unique_lock lock{m_mutex};
        m_client = nullptr;
        m_reconnect_task.schedule_now();
    }
}

void ConsumerImpl::store_current_offset()
{
    std::unique_lock lock{m_mutex};
    if (m_messages_since_last_persist > 0) {
        internal_store_offset(*m_last_seen_offset);
    } else if (m_persist_cursor_task.valid()) {
        m_persist_cursor_task.schedule_after(m_auto_cursor_config->force_persist_delay());
    }
}

void ConsumerImpl::delayed_reconnect()
{
    InternalClientPtr client;
    {
        std::unique_lock lock{m_mutex};
        if (m_status != Status::Started) {
            return;
        }
        client = std::move(m_client);
    }
    if (client != nullptr) {
        client->stop();
    }
    m_reconnect_task.schedule_after(m_recovery_retry_delay);
}

void ConsumerImpl::reconnect_to_server()
{
    {
        std::unique_lock lock{m_mutex};
        if (m_client != nullptr || m_status != Status::Started) {
            return;
        }
    }

    InternalClientPtr client;
    try {
        client = get_client(get_environment());
        client->start();

        std::unique_lock lock{m_mutex};
        if (m_status != Status::Started) {
            // Consumer stopped while we were initiating connection, bail out
            client->stop();
            return;
        }

        if (m_last_seen_offset) {
            m_initial_offset = OffsetSpecification::offset(*m_last_seen_offset + 1);
        }
        client->subscribe(m_subscription_id, m_stream, *m_initial_offset, 10, {});
        m_client = client;
        Logger::info("Consumer recovery successful");
    } catch (const StreamException& e) {
        if (client != nullptr) {
            client->stop();
        }
        if (auto ree = dynamic_cast<const ResponseErrorException*>(&e); ree != nullptr && ree->code() == ResponseCode::StreamDoesNotExist) {
            m_stream_deleted_task.schedule_now();
        } else {
            Logger::warn("Consumer recovery failed, will retry: {}", e.what());
            m_reconnect_task.schedule_after(m_recovery_retry_delay);
        }
    }
}

}  // namespace hareflow::detail