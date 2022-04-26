#include "hareflow/detail/client_impl.h"

#include <fmt/core.h>

#include "hareflow/codec.h"
#include "hareflow/logging.h"
#include "hareflow/detail/commands.h"
#include "hareflow/detail/connection.h"

namespace {

std::uint16_t compute_effective_port(std::uint16_t port, bool use_ssl)
{
    if (port == 0) {
        port = use_ssl ? 5551 : 5552;
    }
    return port;
}

const std::uint16_t MAX_REFERENCE_LENGTH = 256;

const hareflow::Properties DEFAULT_CLIENT_PROPERTIES = {{"product", "RabbitMQ Stream C++ Client"},
                                                        {"platform", "C++"},
                                                        {"version", "0.1.0"},
                                                        {"license", "Apache License Version 2.0"},
                                                        {"source", "https://github.com/coveooss/rabbitmq-stream-cpp-client"}};

class ServerCloseException : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

}  // namespace

namespace hareflow {

ClientPtr Client::create(ClientParameters parameters)
{
    return detail::ClientImpl::create(std::move(parameters));
}

}  // namespace hareflow

namespace hareflow::detail {

const std::map<CommandKey, ClientImpl::HandlerFunc> ClientImpl::FRAME_HANDLERS{
    {CommandKey::Subscribe, &ClientImpl::handle_response<GenericResponse>},
    {CommandKey::Unsubscribe, &ClientImpl::handle_response<GenericResponse>},
    {CommandKey::DeclarePublisher, &ClientImpl::handle_response<GenericResponse>},
    {CommandKey::DeletePublisher, &ClientImpl::handle_response<GenericResponse>},
    {CommandKey::Create, &ClientImpl::handle_response<GenericResponse>},
    {CommandKey::Delete, &ClientImpl::handle_response<GenericResponse>},
    {CommandKey::Open, &ClientImpl::handle_response<OpenResponse>},
    {CommandKey::Metadata, &ClientImpl::handle_response<MetadataResponse>},
    {CommandKey::SaslHandshake, &ClientImpl::handle_response<SaslHandshakeResponse>},
    {CommandKey::SaslAuthenticate, &ClientImpl::handle_response<SaslAuthenticateResponse>},
    {CommandKey::PeerProperties, &ClientImpl::handle_response<PeerPropertiesResponse>},
    {CommandKey::QueryOffset, &ClientImpl::handle_response<QueryOffsetResponse>},
    {CommandKey::QueryPublisherSequence, &ClientImpl::handle_response<QueryPublisherSequenceResponse>},
    {CommandKey::Close, &ClientImpl::handle_close},
    {CommandKey::PublishConfirm, &ClientImpl::handle_publish_confirm},
    {CommandKey::Deliver, &ClientImpl::handle_deliver},
    {CommandKey::PublishError, &ClientImpl::handle_publish_error},
    {CommandKey::MetadataUpdate, &ClientImpl::handle_metadata_update},
    {CommandKey::Tune, &ClientImpl::handle_tune},
    {CommandKey::Credit, &ClientImpl::handle_credit},
    {CommandKey::Heartbeat, &ClientImpl::handle_heartbeat}};

std::shared_ptr<ClientImpl> ClientImpl::create(ClientParameters parameters)
{
    return std::shared_ptr<ClientImpl>(new ClientImpl(std::move(parameters)));
}

ClientImpl::~ClientImpl()
{
    stop();
    if (std::this_thread::get_id() == m_frame_handling_thread.get_id()) {
        m_frame_handling_thread.detach();
    }
}

void ClientImpl::start()
{
    Status expected = Status::Initialized;
    if (!m_status.compare_exchange_strong(expected, Status::Starting)) {
        throw StreamException("Invalid state, did you try calling start() twice?");
    }

    try {
        m_connection->connect();
        m_frame_handling_thread = std::thread([this] { handle_frames(); });
        m_server_properties     = peer_properties();
        authenticate();

        std::future<void> tune_future = m_tune_promise.get_future();
        if (tune_future.wait_for(m_rpc_timeout) == std::future_status::timeout) {
            throw TimeoutException("Timeout while waiting for server tune frame");
        }
        tune_future.get();

        m_connection_properties = open();

        expected = Status::Starting;
        if (!m_status.compare_exchange_strong(expected, Status::Started)) {
            throw StreamException("Client was stopped while start was ongoing");
        }
    } catch (const StreamException&) {
        stop();
        throw;
    }
}

void ClientImpl::stop()
{
    if (Status previous_status = m_status.exchange(Status::Stopping); previous_status != Status::Stopping) {
        if (previous_status == Status::Started) {
            ClientCloseRequest request(++m_correlation_sequence, ResponseCode::Ok, "OK");
            try {
                send_request_check_response_code<GenericResponse>(request);
            } catch (const StreamException& e) {
                Logger::warn("Failed to cleanly stop client, closing connection forcefully: {}", e.what());
            }
            shutdown(ShutdownReason::ClientInitiated);
        } else {
            shutdown(std::nullopt);
        }
    }
    if (std::this_thread::get_id() != m_frame_handling_thread.get_id() && m_frame_handling_thread.joinable()) {
        m_frame_handling_thread.join();
    }
}

std::uint32_t ClientImpl::max_frame_size() const
{
    return m_max_frame_size;
}

StreamMetadataSet ClientImpl::metadata(const std::vector<std::string>& streams)
{
    MetadataRequest request(++m_correlation_sequence, streams);
    auto            response = send_request_wait_response<MetadataResponse>(request);

    std::map<std::uint16_t, BrokerPtr> brokers;
    for (auto& broker : response->get_brokers()) {
        brokers.emplace(broker.m_reference, std::make_shared<Broker>(std::move(broker.m_host), static_cast<std::uint16_t>(broker.m_port)));
    }

    StreamMetadataSet result;
    try {
        for (auto& stream_meta : response->get_stream_metadata()) {
            BrokerPtr              leader;
            std::vector<BrokerPtr> replicas;
            if (static_cast<ResponseCode>(stream_meta.m_code) == ResponseCode::Ok) {
                leader = brokers.at(stream_meta.m_leader);
                for (std::uint16_t reference : stream_meta.m_replicas) {
                    replicas.emplace_back(brokers.at(reference));
                }
            }
            result.emplace(std::move(stream_meta.m_stream_name), static_cast<ResponseCode>(stream_meta.m_code), std::move(leader), std::move(replicas));
        }
    } catch (const std::out_of_range&) {
        throw StreamException("Incoherent results returned by server, failed to compute metadata");
    }
    return result;
}

void ClientImpl::declare_publisher(std::uint8_t publisher_id, std::string_view publisher_reference, std::string_view stream)
{
    if (publisher_reference.size() > MAX_REFERENCE_LENGTH) {
        throw InvalidInputException("Reference max size exceeded");
    }

    DeclarePublisherRequest request(++m_correlation_sequence, publisher_id, publisher_reference, stream);
    send_request_check_response_code<GenericResponse>(request);
}

void ClientImpl::delete_publisher(std::uint8_t publisher_id)
{
    DeletePublisherRequest request(++m_correlation_sequence, publisher_id);
    send_request_check_response_code<GenericResponse>(request);
}

std::optional<std::uint64_t> ClientImpl::query_publisher_sequence(std::string_view publisher_reference, std::string_view stream)
{
    if (publisher_reference.size() > MAX_REFERENCE_LENGTH) {
        throw InvalidInputException("Reference max size exceeded");
    }

    QueryPublisherSequenceRequest request(++m_correlation_sequence, publisher_reference, stream);
    auto                          response = send_request_wait_response<QueryPublisherSequenceResponse>(request);
    if (response->get_response_code() == ResponseCode::Ok) {
        return response->get_sequence();
    } else if (response->get_response_code() == ResponseCode::NoOffset) {
        return std::nullopt;
    } else {
        throw ResponseErrorException(response->get_response_code());
    }
}

void ClientImpl::store_offset(std::string_view reference, std::string_view stream, std::uint64_t offset)
{
    if (reference.size() > MAX_REFERENCE_LENGTH) {
        throw InvalidInputException("Reference max size exceeded");
    }

    StoreOffsetCommand command(reference, stream, offset);
    send_frame(command);
}

std::optional<std::uint64_t> ClientImpl::query_offset(std::string_view reference, std::string_view stream)
{
    if (reference.size() > MAX_REFERENCE_LENGTH) {
        throw InvalidInputException("Reference max size exceeded");
    }

    QueryOffsetRequest request(++m_correlation_sequence, reference, stream);
    auto               response = send_request_wait_response<QueryOffsetResponse>(request);
    if (response->get_response_code() == ResponseCode::Ok) {
        return response->get_offset();
    } else if (response->get_response_code() == ResponseCode::NoOffset) {
        return std::nullopt;
    } else {
        throw ResponseErrorException(response->get_response_code());
    }
}

void ClientImpl::create_stream(std::string_view stream, const Properties& arguments)
{
    CreateRequest request(++m_correlation_sequence, stream, arguments);
    send_request_check_response_code<GenericResponse>(request);
}

void ClientImpl::delete_stream(std::string_view stream)
{
    DeleteRequest request(++m_correlation_sequence, stream);
    send_request_check_response_code<GenericResponse>(request);
}

void ClientImpl::subscribe(std::uint8_t               subscription_id,
                           std::string_view           stream,
                           const OffsetSpecification& offset,
                           std::uint16_t              credit,
                           const Properties&          properties)
{
    SubscribeRequest request(++m_correlation_sequence, subscription_id, stream, offset, credit, properties);
    send_request_check_response_code<GenericResponse>(request);
}

void ClientImpl::unsubscribe(std::uint8_t subscription_id)
{
    UnsubscribeRequest request(++m_correlation_sequence, subscription_id);
    send_request_check_response_code<GenericResponse>(request);
}

void ClientImpl::credit(std::uint8_t subscription_id, std::uint16_t credit)
{
    CreditCommand command(subscription_id, credit);
    send_frame(command);
}

void ClientImpl::publish(std::uint8_t publisher_id, std::uint64_t publishing_id, MessagePtr message)
{
    std::vector<std::uint8_t>            encoded = m_codec->encode(message);
    std::vector<PublishCommand::Message> messages{{publishing_id, boost::asio::buffer(encoded)}};
    PublishCommand                       command(publisher_id, messages);
    send_frame(command);
}

void ClientImpl::publish_encoded(std::uint8_t publisher_id, const std::vector<PublishCommand::Message>& encoded_messages)
{
    std::vector<PublishCommand::Message> message_batch;
    message_batch.reserve(encoded_messages.size());

    std::size_t current_size = PublishCommand::BASE_SERIALIZED_SIZE;
    for (const auto& encoded_message : encoded_messages) {
        std::size_t added_size = encoded_message.serialized_size();
        if (m_max_frame_size > 0 && current_size + added_size > m_max_frame_size) {
            PublishCommand command(publisher_id, message_batch);
            send_frame(command);
            message_batch.clear();
            current_size = PublishCommand::BASE_SERIALIZED_SIZE;
        }

        message_batch.emplace_back(encoded_message);
        current_size += added_size;
    }
    if (!message_batch.empty()) {
        PublishCommand command(publisher_id, message_batch);
        send_frame(command);
    }
}

ClientImpl::ClientImpl(ClientParameters parameters)
 : m_connection{},
   m_user{std::move(parameters.get_user())},
   m_password{std::move(parameters.get_password())},
   m_virtual_host{std::move(parameters.get_virtual_host())},
   m_rpc_timeout{parameters.get_rpc_timeout()},
   m_heartbeat{parameters.get_requested_heartbeat()},
   m_max_frame_size{parameters.get_requested_max_frame_size()},
   m_codec{parameters.get_codec()},
   m_chunk_listener{std::move(parameters.get_chunk_listener())},
   m_message_listener{std::move(parameters.get_message_listener())},
   m_publish_confirm_listener{std::move(parameters.get_publish_confirm_listener())},
   m_publish_error_listener{std::move(parameters.get_publish_error_listener())},
   m_metadata_listener{std::move(parameters.get_metadata_listener())},
   m_credit_error_listener{std::move(parameters.get_credit_error_listener())},
   m_shutdown_listener{std::move(parameters.get_shutdown_listener())},
   m_client_properties{DEFAULT_CLIENT_PROPERTIES},
   m_server_properties{},
   m_connection_properties{},
   m_correlation_sequence{0},
   m_status{Status::Initialized},
   m_tune_promise{},
   m_outstanding_mutex{},
   m_outstanding_requests{},
   m_frame_handling_thread{}
{
    m_client_properties.merge(parameters.get_client_properties());
    m_connection = Connection::create(std::move(parameters.get_host()),
                                      compute_effective_port(parameters.get_port(), parameters.get_use_ssl()),
                                      parameters.get_use_ssl(),
                                      parameters.get_verify_host());
}

void ClientImpl::shutdown(std::optional<ShutdownReason> reason) noexcept
{
    m_connection->shutdown();
    fail_all_outstanding_requests();
    if (m_shutdown_listener && reason) {
        m_shutdown_listener(*reason);
    }
}

Properties ClientImpl::peer_properties()
{
    PeerPropertiesRequest request(++m_correlation_sequence, m_client_properties);
    auto                  response = send_request_check_response_code<PeerPropertiesResponse>(request);
    return std::move(response->get_properties());
}

void ClientImpl::authenticate()
{
    std::vector<std::string> mechanisms = sasl_handshake();

    if (std::find(mechanisms.begin(), mechanisms.end(), "PLAIN") == mechanisms.end()) {
        throw StreamException("Server does not support PLAIN authentication");
    }

    std::vector<std::uint8_t> sasl_blob;
    sasl_blob.reserve(m_user.size() + m_password.size() + 2);
    sasl_blob.push_back(0);
    sasl_blob.insert(sasl_blob.end(), m_user.begin(), m_user.end());
    sasl_blob.push_back(0);
    sasl_blob.insert(sasl_blob.end(), m_password.begin(), m_password.end());

    SaslAuthenticateRequest request(++m_correlation_sequence, "PLAIN", sasl_blob);
    send_request_check_response_code<SaslAuthenticateResponse>(request);
}

std::vector<std::string> ClientImpl::sasl_handshake()
{
    SaslHandshakeRequest request(++m_correlation_sequence);
    auto                 response = send_request_check_response_code<SaslHandshakeResponse>(request);
    return std::move(response->get_mechanisms());
}

Properties ClientImpl::open()
{
    OpenRequest request(++m_correlation_sequence, m_virtual_host);
    auto        response = send_request_check_response_code<OpenResponse>(request);
    return std::move(response->get_connection_properties());
}

void ClientImpl::complete_outstanding_request(std::unique_ptr<ServerResponse> response)
{
    std::unique_lock lock(m_outstanding_mutex);
    if (auto it = m_outstanding_requests.find(response->get_correlation_id()); it != m_outstanding_requests.end()) {
        it->second.set_value(std::move(response));
        m_outstanding_requests.erase(it);
    } else {
        Logger::warn("Could not find correlation id {} in outstanding requests.", response->get_correlation_id());
    }
}

void ClientImpl::drop_outstanding_request(std::uint32_t correlation_id)
{
    std::unique_lock lock(m_outstanding_mutex);
    m_outstanding_requests.erase(correlation_id);
}

void ClientImpl::fail_all_outstanding_requests()
{
    std::unique_lock lock(m_outstanding_mutex);
    for (auto& [correlation_id, promise] : m_outstanding_requests) {
        promise.set_exception(std::make_exception_ptr(IOException("Connection shutdown while waiting for response")));
    }
    m_outstanding_requests.clear();
}

ClientImpl::ResponseFuture ClientImpl::send_request(const ClientRequest& request)
{
    ResponseFuture response_future;
    {
        std::unique_lock lock(m_outstanding_mutex);
        response_future = m_outstanding_requests.try_emplace(request.get_correlation_id()).first->second.get_future();
    }
    send_frame(request).get();
    return response_future;
}

std::future<void> ClientImpl::send_frame(const Serializable& frame)
{
    std::size_t frame_data_size = frame.serialized_size();
    std::size_t frame_size      = frame_data_size + sizeof(std::uint32_t);
    if (m_max_frame_size > 0 && frame_size > m_max_frame_size) {
        throw InvalidInputException(fmt::format("Maximum frame size exceeded ({})", m_max_frame_size));
    }

    BinaryBuffer buffer = m_connection->allocate_buffer(frame_size);
    buffer.write_uint(static_cast<std::uint32_t>(frame_data_size));
    frame.serialize(buffer);

    return m_connection->write(std::move(buffer));
}

void ClientImpl::handle_frames()
{
    std::shared_ptr<ClientImpl> self;
    ShutdownReason              shutdown_reason;

    while (true) {
        BinaryBuffer buffer;
        try {
            buffer = m_connection->read().get();

            std::uint16_t key         = buffer.read_ushort();
            std::uint16_t version     = buffer.read_ushort();
            bool          is_response = (key & 0x8000) != 0;
            key &= 0x7FFF;

            if (version != 1) {
                throw StreamException(fmt::format("Unsupported command version {}", version));
            }

            if (m_status.load() != Status::Stopping) {
                if (auto it = FRAME_HANDLERS.find(static_cast<CommandKey>(key)); it != FRAME_HANDLERS.end()) {
                    std::invoke(it->second, this, buffer);
                } else {
                    throw StreamException(fmt::format("Unsupported command {:#04x}", key));
                }
            } else if (is_response && static_cast<CommandKey>(key) == CommandKey::Close) {
                handle_response<GenericResponse>(buffer);
                shutdown_reason = ShutdownReason::ClientInitiated;
                break;
            } else {
                Logger::debug("Skipping frame for command {:#04x}, waiting for response to close request", key);
            }
        } catch (const ServerCloseException&) {
            shutdown_reason = ShutdownReason::ServerInitiated;
            break;
        } catch (const IOException&) {
            shutdown_reason = ShutdownReason::Unknown;
            break;
        } catch (const StreamException& e) {
            Logger::warn("Exception while processing frame: {}", e.what());
        }

        m_connection->release_buffer(std::move(buffer));
    }

    Status expected = Status::Started;
    if (m_status.compare_exchange_strong(expected, Status::Stopping)) {
        if (shutdown_reason == ShutdownReason::ServerInitiated) {
            Logger::info("Server requested close, shutting down connection");
        } else if (shutdown_reason == ShutdownReason::Unknown) {
            Logger::warn("Connection failed unexpectedly");
        }

        // Make sure the client lives until the end of the method, since shutdown callback might drop last living reference
        self = shared_from_this();
        shutdown(shutdown_reason);
    }
}

void ClientImpl::handle_close(BinaryBuffer& buffer)
{
    ServerCloseRequest request;
    request.deserialize(buffer);

    ClientCloseResponse response(request.get_correlation_id(), ResponseCode::Ok);
    send_frame(response);

    throw ServerCloseException("Server requested close");
}

void ClientImpl::handle_deliver(BinaryBuffer& buffer)
{
    if (!m_chunk_listener && !m_message_listener) {
        return;
    }

    DeliverCommand command;
    command.deserialize(buffer);
    std::uint8_t                 subscription_id = command.get_subscription_id();
    const DeliverCommand::Chunk& chunk           = command.get_chunk();
    if (m_chunk_listener) {
        m_chunk_listener(*this, subscription_id, chunk.m_timestamp, chunk.m_offset, chunk.m_nb_entries);
    }
    if (!m_message_listener) {
        return;
    }

    std::uint64_t message_offset = chunk.m_offset;
    for (const auto& encoded_message : command.get_messages()) {
        MessagePtr message = m_codec->decode(encoded_message.data(), encoded_message.size());
        m_message_listener(subscription_id, chunk.m_timestamp, message_offset, message);
        ++message_offset;
    }
}

void ClientImpl::handle_publish_confirm(BinaryBuffer& buffer)
{
    if (!m_publish_confirm_listener) {
        return;
    }

    PublishConfirmCommand command;
    command.deserialize(buffer);

    std::uint8_t publisher_id = command.get_publisher_id();
    for (std::uint64_t publishing_id : command.get_sequences()) {
        m_publish_confirm_listener(publisher_id, publishing_id);
    }
}

void ClientImpl::handle_publish_error(BinaryBuffer& buffer)
{
    if (!m_publish_error_listener) {
        return;
    }

    PublishErrorCommand command;
    command.deserialize(buffer);

    std::uint8_t publisher_id = command.get_publisher_id();
    for (const auto& publish_error : command.get_publishing_errors()) {
        m_publish_error_listener(publisher_id, publish_error.m_sequence, static_cast<ResponseCode>(publish_error.m_code));
    }
}

void ClientImpl::handle_tune(BinaryBuffer& buffer)
{
    ServerTuneCommand command;
    command.deserialize(buffer);

    auto negociate_value = [](std::uint32_t client, std::uint32_t server) {
        return client == 0 || server == 0 ? std::max(client, server) : std::min(client, server);
    };
    std::uint32_t heartbeat_seconds = static_cast<std::uint32_t>(m_heartbeat.count());
    heartbeat_seconds               = negociate_value(heartbeat_seconds, command.get_heartbeat());
    m_max_frame_size                = negociate_value(m_max_frame_size, command.get_frame_size());
    m_heartbeat                     = std::chrono::seconds(heartbeat_seconds);

    send_frame(ClientTuneCommand(m_max_frame_size, heartbeat_seconds));

    if (m_heartbeat != std::chrono::seconds::zero()) {
        m_connection->set_writer_idle_handler(m_heartbeat, [this]() { send_frame(ClientHeartbeatCommand()); });
        m_connection->set_reader_idle_handler(m_heartbeat * 2, [this]() {
            Status expected = Status::Started;
            if (m_status.compare_exchange_strong(expected, Status::Stopping)) {
                Logger::warn("Timeout while waiting for server heartbeat, closing connection");
                shutdown(ShutdownReason::HeartbeatTimeout);
            }
        });
    }
    m_tune_promise.set_value();
}

void ClientImpl::handle_metadata_update(BinaryBuffer& buffer)
{
    MetadataUpdateCommand command;
    command.deserialize(buffer);

    if (command.get_response_code() != ResponseCode::StreamNotAvailable) {
        throw ResponseErrorException(command.get_response_code());
    }

    if (m_metadata_listener) {
        m_metadata_listener(command.get_stream_name(), command.get_response_code());
    }
}

void ClientImpl::handle_credit(BinaryBuffer& buffer)
{
    if (!m_credit_error_listener) {
        return;
    }

    CreditErrorCommand command;
    command.deserialize(buffer);

    m_credit_error_listener(command.get_subscription_id(), command.get_response_code());
}

void ClientImpl::handle_heartbeat(BinaryBuffer& /*buffer*/)
{
    // Nothing to do, server idle timer already reset
}

}  // namespace hareflow::detail
