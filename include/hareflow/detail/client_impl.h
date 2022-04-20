#pragma once

#include "hareflow/client.h"
#include "hareflow/client_parameters.h"
#include "hareflow/detail/commands.h"
#include "hareflow/detail/internal_types.h"

namespace hareflow::detail {

class InternalClient : public Client
{
public:
    virtual ~InternalClient() = default;

    virtual std::uint32_t max_frame_size() const                                                                           = 0;
    virtual void          publish_encoded(std::uint8_t publisher_id, const std::vector<PublishCommand::Message>& messages) = 0;
};

class ClientImpl final : public InternalClient, public std::enable_shared_from_this<ClientImpl>
{
public:
    static std::shared_ptr<ClientImpl> create(ClientParameters parameters);
    virtual ~ClientImpl();

    void start() override;
    void stop() override;

    std::uint32_t max_frame_size() const override;

    StreamMetadataSet            metadata(const std::vector<std::string>& streams) override;
    void                         declare_publisher(std::uint8_t publisher_id, std::string_view publisher_reference, std::string_view stream) override;
    void                         delete_publisher(std::uint8_t publisher_id) override;
    std::optional<std::uint64_t> query_publisher_sequence(std::string_view publisher_reference, std::string_view stream) override;
    void                         store_offset(std::string_view reference, std::string_view stream, std::uint64_t offset) override;
    std::optional<std::uint64_t> query_offset(std::string_view reference, std::string_view stream) override;
    void                         create_stream(std::string_view stream, const Properties& arguments) override;
    void                         delete_stream(std::string_view stream) override;
    void                         subscribe(std::uint8_t               subscription_id,
                                           std::string_view           stream,
                                           const OffsetSpecification& offset,
                                           std::uint16_t              credit,
                                           const Properties&          properties) override;
    void                         unsubscribe(std::uint8_t subscription_id) override;
    void                         credit(std::uint8_t subscription_id, std::uint16_t credit) override;
    void                         publish(std::uint8_t publisher_id, std::uint64_t publishing_id, MessagePtr message) override;
    void                         publish_encoded(std::uint8_t publisher_id, const std::vector<PublishCommand::Message>& messages) override;

private:
    enum class Status { Initialized, Starting, Started, Stopping };
    using ResponsePromise = std::promise<std::unique_ptr<ServerResponse>>;
    using ResponseFuture  = std::future<std::unique_ptr<ServerResponse>>;

    ClientImpl(ClientParameters parameters);

    void                     shutdown(std::optional<ShutdownReason> reason) noexcept;
    Properties               peer_properties();
    void                     authenticate();
    std::vector<std::string> sasl_handshake();
    Properties               open();

    void complete_outstanding_request(std::unique_ptr<ServerResponse> response);
    void drop_outstanding_request(std::uint32_t correlation_id);
    void fail_all_outstanding_requests();

    template<typename ResponseType> std::unique_ptr<ResponseType> send_request_check_response_code(const ClientRequest& request);
    template<typename ResponseType> std::unique_ptr<ResponseType> send_request_wait_response(const ClientRequest& request);
    ResponseFuture                                                send_request(const ClientRequest& request);
    std::future<void>                                             send_frame(const Serializable& frame);

    void                                 handle_frames();
    template<typename ResponseType> void handle_response(BinaryBuffer& buffer);
    void                                 handle_close(BinaryBuffer& buffer);
    void                                 handle_deliver(BinaryBuffer& buffer);
    void                                 handle_publish_confirm(BinaryBuffer& buffer);
    void                                 handle_publish_error(BinaryBuffer& buffer);
    void                                 handle_tune(BinaryBuffer& buffer);
    void                                 handle_metadata_update(BinaryBuffer& buffer);
    void                                 handle_credit(BinaryBuffer& buffer);
    void                                 handle_heartbeat(BinaryBuffer& buffer);

    std::shared_ptr<Connection> m_connection;

    std::string                m_user;
    std::string                m_password;
    std::string                m_virtual_host;
    std::chrono::seconds       m_rpc_timeout;
    std::chrono::seconds       m_heartbeat;
    std::uint32_t              m_max_frame_size;
    CodecPtr                   m_codec;
    ChunkListener              m_chunk_listener;
    MessageListener            m_message_listener;
    PublishConfirmListener     m_publish_confirm_listener;
    PublishErrorListener       m_publish_error_listener;
    MetadataListener           m_metadata_listener;
    CreditErrorListener        m_credit_error_listener;
    ShutdownListener           m_shutdown_listener;
    Properties                 m_client_properties;
    Properties                 m_server_properties;
    Properties                 m_connection_properties;
    std::atomic<std::uint32_t> m_correlation_sequence;
    std::atomic<Status>        m_status;

    std::promise<void> m_tune_promise;

    std::mutex                               m_outstanding_mutex;
    std::map<std::uint32_t, ResponsePromise> m_outstanding_requests;

    std::thread m_frame_handling_thread;

    using HandlerFunc = void (ClientImpl::*)(BinaryBuffer&);
    static const std::map<CommandKey, HandlerFunc> FRAME_HANDLERS;
};

}  // namespace hareflow::detail

#include "client_impl.hpp"