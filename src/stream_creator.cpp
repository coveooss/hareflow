#include "hareflow/stream_creator.h"

#include <map>

#include <fmt/core.h>

#include "hareflow/exceptions.h"
#include "hareflow/detail/client_impl.h"
#include "hareflow/detail/environment_impl.h"

namespace {

using namespace hareflow;
const std::map<hareflow::LeaderLocator, std::string> LOCATOR_TO_STRING{{LeaderLocator::LeastLeaders, "least-leaders"},
                                                                       {LeaderLocator::ClientLocal, "client-local"},
                                                                       {LeaderLocator::Random, "random"}};

}  // namespace

namespace hareflow {

StreamCreator::StreamCreator(detail::InternalEnvironmentPtr environment) : m_environment{environment}
{
    leader_locator(LeaderLocator::LeastLeaders);
}

StreamCreator& StreamCreator::stream(std::string stream) &
{
    m_stream = std::move(stream);
    return *this;
}

StreamCreator& StreamCreator::max_length(std::uint64_t bytes) &
{
    m_properties["max-length-bytes"] = std::to_string(bytes);
    return *this;
}

StreamCreator& StreamCreator::max_segment_size(std::uint64_t bytes) &
{
    m_properties["max-segment-size-bytes"] = std::to_string(bytes);
    return *this;
}

StreamCreator& StreamCreator::max_age(std::chrono::seconds max_age) &
{
    m_properties["max-age"] = fmt::format("{}s", max_age.count());
    return *this;
}

StreamCreator& StreamCreator::leader_locator(LeaderLocator leader_locator) &
{
    m_properties["queue-leader-locator"] = LOCATOR_TO_STRING.at(leader_locator);
    return *this;
}

StreamCreator StreamCreator::stream(std::string stream) &&
{
    return std::move(this->stream(std::move(stream)));
}

StreamCreator StreamCreator::max_length(std::uint64_t bytes) &&
{
    return std::move(this->max_length(bytes));
}

StreamCreator StreamCreator::max_segment_size(std::uint64_t bytes) &&
{
    return std::move(this->max_segment_size(bytes));
}

StreamCreator StreamCreator::max_age(std::chrono::seconds max_age) &&
{
    return std::move(this->max_age(max_age));
}

StreamCreator StreamCreator::leader_locator(LeaderLocator leader_locator) &&
{
    return std::move(this->leader_locator(leader_locator));
}

void StreamCreator::create()
{
    if (m_stream.empty()) {
        throw InvalidInputException("stream needs to be specified for a creator");
    }
    m_environment->locator_operation([this](Client& client) { client.create_stream(m_stream, m_properties); });
}

}  // namespace hareflow