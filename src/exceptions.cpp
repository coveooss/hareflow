#include "hareflow/exceptions.h"

#include <fmt/core.h>

namespace {

using namespace hareflow;
const std::map<ResponseCode, std::string> RESPONSE_CODES_TO_STRING = {{ResponseCode::Ok, "Ok"},
                                                                      {ResponseCode::StreamDoesNotExist, "StreamDoesNotExist"},
                                                                      {ResponseCode::SubscriptionIdAlreadyExists, "SubscriptionIdAlreadyExists"},
                                                                      {ResponseCode::SubscriptionIdDoesNotExist, "SubscriptionIdDoesNotExist"},
                                                                      {ResponseCode::StreamAlreadyExists, "StreamAlreadyExists"},
                                                                      {ResponseCode::StreamNotAvailable, "StreamNotAvailable"},
                                                                      {ResponseCode::SaslMechanismNotSupported, "SaslMechanismNotSupported"},
                                                                      {ResponseCode::AuthenticationFailure, "AuthenticationFailure"},
                                                                      {ResponseCode::SaslError, "SaslError"},
                                                                      {ResponseCode::SaslChallenge, "SaslChallenge"},
                                                                      {ResponseCode::SaslAuthenticationFailureLoopback, "SaslAuthenticationFailureLoopback"},
                                                                      {ResponseCode::VirtualHostAccessFailure, "VirtualHostAccessFailure"},
                                                                      {ResponseCode::UnknownFrame, "UnknownFrame"},
                                                                      {ResponseCode::FrameTooLarge, "FrameTooLarge"},
                                                                      {ResponseCode::InternalError, "InternalError"},
                                                                      {ResponseCode::AccessRefused, "AccessRefused"},
                                                                      {ResponseCode::PreconditionFailed, "PreconditionFailed"},
                                                                      {ResponseCode::PublisherDoesNotExist, "PublisherDoesNotExist"},
                                                                      {ResponseCode::NoOffset, "NoOffset"}};

std::string build_what(ResponseCode code)
{
    static constexpr std::string_view format_str = "Error response received from server: {}";
    if (auto it = RESPONSE_CODES_TO_STRING.find(code); it != RESPONSE_CODES_TO_STRING.end()) {
        return fmt::format(format_str, it->second);
    } else {
        return fmt::format(format_str, fmt::format("{:#x}", static_cast<std::uint16_t>(code)));
    }
}

}  // namespace

namespace hareflow {

ResponseErrorException::ResponseErrorException(ResponseCode code) : StreamException(build_what(code)), m_code(code)
{
}

}  // namespace hareflow