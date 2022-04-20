#include "hareflow/detail/producer_impl.h"

#include <map>

#include "hareflow/logging.h"
#include "hareflow/detail/accumulator.h"
#include "hareflow/detail/client_impl.h"
#include "hareflow/detail/commands.h"
#include "hareflow/detail/environment_impl.h"

namespace hareflow::detail {

std::shared_ptr<ProducerImpl> ProducerImpl::create(std::string               name,
                                                   std::string               stream,
                                                   std::uint32_t             batch_size,
                                                   std::chrono::milliseconds batch_publishing_delay,
                                                   std::uint32_t             max_unconfirmed,
                                                   std::chrono::milliseconds confirm_timeout,
                                                   std::chrono::milliseconds enqueue_timeout,
                                                   InternalEnvironmentPtr    environment)
{
    return std::shared_ptr<ProducerImpl>{new ProducerImpl{std::move(name),
                                                          std::move(stream),
                                                          batch_size,
                                                          batch_publishing_delay,
                                                          max_unconfirmed,
                                                          confirm_timeout,
                                                          enqueue_timeout,
                                                          std::move(environment)}};
}

ProducerImpl::~ProducerImpl()
{
    stop();
}

void ProducerImpl::start()
{
    Status expected = Status::Initialized;
    if (!m_status.compare_exchange_strong(expected, Status::Starting)) {
        throw StreamException("Invalid status for start request");
    }

    std::unique_lock lock{m_mutex};

    auto environment = get_environment();
    environment->register_producer(weak_from_this());
    m_client = get_client(environment);

    m_client->start();
    m_accumulator->set_max_frame_size(m_client->max_frame_size());
    m_client->declare_publisher(m_publisher_id, m_name, m_stream);
    if (!m_name.empty()) {
        std::optional<std::uint64_t> last_published = m_client->query_publisher_sequence(m_name, m_stream);
        m_next_publishing_id                        = last_published ? *last_published + 1 : 0;
    }
    if (m_batch_publish_task.valid()) {
        m_batch_publish_task.schedule_after(m_batch_publishing_delay);
    }
    if (m_confirm_timeout_task.valid()) {
        m_confirm_timeout_task.schedule_after(std::chrono::seconds(1));
    }

    expected = Status::Starting;
    if (!m_status.compare_exchange_strong(expected, Status::Started)) {
        throw StreamException("Producer was stopped while start was ongoing");
    }
}

void ProducerImpl::stop()
{
    m_stream_deleted_task.destroy();
    internal_stop(static_cast<std::uint16_t>(ProducerErrorCode::ProducerStopped));
}

void ProducerImpl::send(MessagePtr message, ConfirmationHandler confirmation_handler)
{
    send(m_next_publishing_id++, std::move(message), std::move(confirmation_handler));
}

void ProducerImpl::send(std::uint64_t publishing_id, MessagePtr message, ConfirmationHandler confirmation_handler)
{
    if (m_status.load() != Status::Started) {
        throw ProducerException{ProducerErrorCode::ProducerStopped, "Producer is stopped"};
    }

    try {
        if (m_enqueue_timeout == std::chrono::milliseconds::zero()) {
            m_enqueue_semaphore.acquire();
        } else if (!m_enqueue_semaphore.try_acquire_for(m_enqueue_timeout)) {
            throw ProducerException{ProducerErrorCode::MessageEnqueuingFailed, "Enqueueing message timeout"};
        }

        if (m_accumulator->add(publishing_id, std::move(message), std::move(confirmation_handler))) {
            publish_batch();
        }
    } catch (const SemaphoreDestroyedException&) {
        throw ProducerException{ProducerErrorCode::ProducerStopped, "Producer is stopped"};
    } catch (const AccumulatorDestroyedException&) {
        throw ProducerException{ProducerErrorCode::ProducerStopped, "Producer is stopped"};
    }
}

void ProducerImpl::flush()
{
    if (m_status.load() != Status::Started) {
        throw ProducerException{ProducerErrorCode::ProducerStopped, "Producer is stopped"};
    }
    publish_batch();
}

ProducerImpl::ProducerImpl(std::string               name,
                           std::string               stream,
                           std::uint32_t             batch_size,
                           std::chrono::milliseconds batch_publishing_delay,
                           std::uint32_t             max_unconfirmed,
                           std::chrono::milliseconds confirm_timeout,
                           std::chrono::milliseconds enqueue_timeout,
                           InternalEnvironmentPtr    environment)
 : m_name{std::move(name)},
   m_stream{std::move(stream)},
   m_batch_size{batch_size},
   m_batch_publishing_delay{batch_publishing_delay},
   m_confirm_timeout{confirm_timeout},
   m_enqueue_timeout{enqueue_timeout},
   m_recovery_retry_delay{environment->get_recovery_retry_delay()},
   m_environment{environment},
   m_next_publishing_id{0},
   m_accumulator{},
   m_status{Status::Initialized},
   m_mutex{},
   m_publisher_id{0},  // No multiplexing for now
   m_enqueue_semaphore{max_unconfirmed},
   m_unconfirmed{1000},
   m_client{},
   m_batch_publish_task{},
   m_confirm_timeout_task{},
   m_reconnect_task{},
   m_delayed_reconnect_task{}
{
    m_accumulator = std::make_unique<Accumulator>(batch_size, environment->get_codec());
    if (batch_publishing_delay != std::chrono::milliseconds::zero()) {
        m_batch_publish_task = ReschedulableTask{environment->background_scheduler(), [this]() { publish_batch(); }};
    }
    if (m_confirm_timeout != std::chrono::milliseconds::zero()) {
        m_confirm_timeout_task = ReschedulableTask{environment->background_scheduler(), [this]() { check_confirm_timeout(); }};
    }
    m_reconnect_task         = ReschedulableTask{environment->background_scheduler(), [this]() { reconnect_to_server(); }};
    m_delayed_reconnect_task = ReschedulableTask{environment->background_scheduler(), [this]() { delayed_reconnect(); }};
    m_stream_deleted_task =
        ReschedulableTask{environment->background_scheduler(), [this]() { internal_stop(static_cast<std::uint16_t>(ResponseCode::StreamDoesNotExist)); }};
}

InternalEnvironmentPtr ProducerImpl::get_environment()
{
    auto environment = m_environment.lock();
    if (environment == nullptr) {
        throw StreamException{"Invalid state: environment destroyed while producer is still alive"};
    }
    return environment;
}

InternalClientPtr ProducerImpl::get_client(InternalEnvironmentPtr environment)
{
    namespace ph = std::placeholders;

    ClientParameters parameters = environment->get_base_client_parameters()
                                      .with_publish_confirm_listener(std::bind(&ProducerImpl::handle_publish_confirm, this, ph::_2))
                                      .with_publish_error_listener(std::bind(&ProducerImpl::handle_publish_error, this, ph::_2, ph::_3))
                                      .with_metadata_listener(std::bind(&ProducerImpl::handle_metadata_update, this, ph::_1))
                                      .with_shutdown_listener(std::bind(&ProducerImpl::handle_shutdown, this, ph::_1));
    return environment->get_producer_client(m_stream, std::move(parameters));
}

void ProducerImpl::internal_stop(std::uint16_t code)
{
    m_batch_publish_task.destroy();
    m_confirm_timeout_task.destroy();
    m_delayed_reconnect_task.destroy();
    m_reconnect_task.destroy();

    if (m_status.exchange(Status::Stopped) != Status::Stopped) {
        m_accumulator->destroy();
        m_enqueue_semaphore.destroy();

        InternalClientPtr client;
        {
            std::unique_lock lock{m_mutex};
            client = std::move(m_client);
        }
        if (client != nullptr) {
            try {
                client->delete_publisher(m_publisher_id);
            } catch (const StreamException& e) {
                Logger::warn("Failed to cleanly delete publisher while stopping producer: {}", e.what());
            }
            client->stop();
        }
        fail_all_unconfirmed(code);

        if (auto environment = m_environment.lock()) {
            environment->unregister_producer(weak_from_this());
        }
    }
}

void ProducerImpl::fail_all_unconfirmed(std::uint16_t code)
{
    std::unique_lock lock{m_mutex};
    for (auto& [publishing_id, message] : m_unconfirmed) {
        if (message->confirmation_handler) {
            message->confirmation_handler(ConfirmationStatus{publishing_id, message->message, false, code});
        }
    }
    m_unconfirmed.clear();

    for (auto& message : m_accumulator->extract_all()) {
        if (message->confirmation_handler) {
            message->confirmation_handler(ConfirmationStatus{message->publishing_id, message->message, false, code});
        }
    }
}

void ProducerImpl::handle_publish_confirm(std::uint64_t publishing_id)
{
    AccumulatedMessagePtr message;
    {
        std::unique_lock lock{m_mutex};
        if (auto it = m_unconfirmed.find(publishing_id); it != m_unconfirmed.end()) {
            message = std::move(it->second);
            m_unconfirmed.erase(it);
        }
    }
    if (message != nullptr && message->confirmation_handler != nullptr) {
        message->confirmation_handler(ConfirmationStatus{publishing_id, message->message, true, static_cast<std::uint16_t>(ResponseCode::Ok)});
    }
    m_enqueue_semaphore.release();
}

void ProducerImpl::handle_publish_error(std::uint64_t publishing_id, ResponseCode error_code)
{
    AccumulatedMessagePtr message;
    {
        std::unique_lock lock{m_mutex};
        if (auto it = m_unconfirmed.find(publishing_id); it != m_unconfirmed.end()) {
            message = std::move(it->second);
            m_unconfirmed.erase(it);
        }
    }
    if (message != nullptr && message->confirmation_handler != nullptr) {
        message->confirmation_handler(ConfirmationStatus{publishing_id, message->message, false, static_cast<std::uint16_t>(error_code)});
    }
    m_enqueue_semaphore.release();
}

void ProducerImpl::handle_metadata_update(std::string_view stream)
{
    if (stream == m_stream) {
        Logger::info("Triggering delayed producer recovery because of stream metadata update");
        m_delayed_reconnect_task.schedule_now();
    }
}

void ProducerImpl::handle_shutdown(ShutdownReason reason)
{
    if (reason != ShutdownReason::ClientInitiated && m_status == Status::Started) {
        Logger::info("Triggering producer recovery because of unexpected client shutdown");
        std::unique_lock lock{m_mutex};
        m_client = nullptr;
        m_reconnect_task.schedule_now();
    }
}

void ProducerImpl::publish_batch()
{
    std::unique_lock lock{m_mutex};
    if (m_status != Status::Started) {
        return;
    }

    std::vector<AccumulatedMessagePtr> messages = m_accumulator->extract_all();
    if (!messages.empty()) {
        // Skip sending if client isn't connected, will be sent on reconnect
        if (m_client != nullptr) {
            std::vector<PublishCommand::Message> encoded_messages;
            encoded_messages.reserve(messages.size());
            for (const auto& message : messages) {
                encoded_messages.emplace_back(PublishCommand::Message{message->publishing_id, boost::asio::buffer(message->encoded_message)});
            }

            try {
                m_client->publish_encoded(m_publisher_id, encoded_messages);
            } catch (const StreamException& e) {
                Logger::warn("Failed to publish messages: {}", e.what());
            }
        }

        for (const auto& message : messages) {
            m_unconfirmed.emplace(message->publishing_id, message);
        }
    }
    if (m_batch_publish_task.valid()) {
        m_batch_publish_task.schedule_after(m_batch_publishing_delay);
    }
}

void ProducerImpl::check_confirm_timeout()
{
    std::vector<AccumulatedMessagePtr> timed_out;
    ProducerErrorCode                  error_code;
    {
        std::unique_lock                               lock{m_mutex};
        std::map<std::uint64_t, AccumulatedMessagePtr> sorted_unconfirmed{m_unconfirmed.begin(), m_unconfirmed.end()};
        std::chrono::steady_clock::time_point          timeout_threshold = std::chrono::steady_clock::now() - m_confirm_timeout;
        for (auto& [publishing_id, message] : sorted_unconfirmed) {
            if (message->publish_time > timeout_threshold) {
                // ordered chronologically, so we're done
                break;
            }
            timed_out.emplace_back(std::move(message));
            m_unconfirmed.erase(publishing_id);
        }
        error_code = m_client == nullptr ? ProducerErrorCode::ProducerNotAvailable : ProducerErrorCode::ConfirmTimeout;
    }
    for (auto& message : timed_out) {
        if (message->confirmation_handler != nullptr) {
            message->confirmation_handler(ConfirmationStatus{message->publishing_id, message->message, false, static_cast<std::uint16_t>(error_code)});
        }
    }
    m_confirm_timeout_task.schedule_after(std::chrono::seconds(1));
}

void ProducerImpl::delayed_reconnect()
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

void ProducerImpl::reconnect_to_server()
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
        client->declare_publisher(m_publisher_id, m_name, m_stream);

        std::unique_lock lock{m_mutex};
        if (m_status != Status::Started) {
            // Producer stopped while we were initiating connection, bail out
            return;
        }

        std::map<std::uint64_t, AccumulatedMessagePtr> sorted_unconfirmed{m_unconfirmed.begin(), m_unconfirmed.end()};
        std::vector<PublishCommand::Message>           encoded_messages;
        encoded_messages.reserve(m_batch_size);
        for (const auto& [publishing_id, message] : sorted_unconfirmed) {
            encoded_messages.emplace_back(PublishCommand::Message{publishing_id, boost::asio::buffer(message->encoded_message)});
            if (encoded_messages.size() == m_batch_size) {
                client->publish_encoded(m_publisher_id, encoded_messages);
                encoded_messages.clear();
            }
        }
        if (!encoded_messages.empty()) {
            client->publish_encoded(m_publisher_id, encoded_messages);
        }
        m_client = client;
        Logger::info("Producer recovery successful");
    } catch (const StreamException& e) {
        if (client != nullptr) {
            client->stop();
        }
        if (auto ree = dynamic_cast<const ResponseErrorException*>(&e); ree != nullptr && ree->code() == ResponseCode::StreamDoesNotExist) {
            m_stream_deleted_task.schedule_now();
        } else {
            Logger::warn("Producer recovery failed, will retry: {}", e.what());
            m_reconnect_task.schedule_after(m_recovery_retry_delay);
        }
    }
}

}  // namespace hareflow::detail