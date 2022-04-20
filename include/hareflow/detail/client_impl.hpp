namespace hareflow::detail {

template<typename ResponseType> std::unique_ptr<ResponseType> ClientImpl::send_request_check_response_code(const ClientRequest& request)
{
    auto response = send_request_wait_response<ResponseType>(request);
    if (response->get_response_code() != ResponseCode::Ok) {
        throw ResponseErrorException(response->get_response_code());
    }
    return response;
}

template<typename ResponseType> std::unique_ptr<ResponseType> ClientImpl::send_request_wait_response(const ClientRequest& request)
{
    std::unique_ptr<ServerResponse> untyped_response;
    try {
        ResponseFuture response_future = send_request(request);
        if (response_future.wait_for(m_rpc_timeout) == std::future_status::timeout) {
            throw TimeoutException("Timeout while waiting for response");
        }

        untyped_response = response_future.get();
    } catch (const StreamException&) {
        drop_outstanding_request(request.get_correlation_id());
        throw;
    }

    ResponseType* cast_response = dynamic_cast<ResponseType*>(untyped_response.get());
    if (cast_response == nullptr) {
        throw StreamException("Unexpected type for response object");
    }
    untyped_response.release();
    return std::unique_ptr<ResponseType>(cast_response);
}

template<typename ResponseType> void ClientImpl::handle_response(BinaryBuffer& buffer)
{
    auto response = std::make_unique<ResponseType>();
    response->deserialize(buffer);
    complete_outstanding_request(std::move(response));
}

}  // namespace hareflow::detail