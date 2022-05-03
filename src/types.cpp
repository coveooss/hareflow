#include "hareflow/types.h"

#include "hareflow/exceptions.h"

namespace {

const std::uint16_t OFFSET_FIRST     = 1;
const std::uint16_t OFFSET_LAST      = 2;
const std::uint16_t OFFSET_NEXT      = 3;
const std::uint16_t OFFSET_OFFSET    = 4;
const std::uint16_t OFFSET_TIMESTAMP = 5;

}  // namespace

namespace hareflow {

OffsetSpecification OffsetSpecification::first()
{
    return OffsetSpecification(OFFSET_FIRST, 0);
}

OffsetSpecification OffsetSpecification::last()
{
    return OffsetSpecification(OFFSET_LAST, 0);
}

OffsetSpecification OffsetSpecification::next()
{
    return OffsetSpecification(OFFSET_NEXT, 0);
}

OffsetSpecification OffsetSpecification::offset(std::uint64_t offset)
{
    return OffsetSpecification(OFFSET_OFFSET, offset);
}

OffsetSpecification OffsetSpecification::timestamp(std::chrono::system_clock::time_point timestamp)
{
    std::chrono::milliseconds since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch());
    return OffsetSpecification::timestamp(since_epoch.count());
}

OffsetSpecification OffsetSpecification::timestamp(std::int64_t timestamp)
{
    if (timestamp < 0) {
        throw InvalidInputException("Specified timestamp is before epoch");
    }
    return OffsetSpecification(OFFSET_TIMESTAMP, static_cast<std::uint64_t>(timestamp));
}

std::uint16_t OffsetSpecification::get_type() const
{
    return m_type;
}

std::uint64_t OffsetSpecification::get_offset() const
{
    return m_offset;
}

bool OffsetSpecification::is_offset() const
{
    return m_type == OFFSET_OFFSET;
}

bool OffsetSpecification::is_timestamp() const
{
    return m_type == OFFSET_TIMESTAMP;
}

bool OffsetSpecification::operator==(const OffsetSpecification& other) const
{
    return std::tie(m_type, m_offset) == std::tie(other.m_type, other.m_offset);
}

OffsetSpecification::OffsetSpecification(std::uint16_t type, std::uint64_t offset) : m_type(type), m_offset(offset)
{
}

Broker::Broker(std::string host, std::uint16_t port) : m_host(std::move(host)), m_port(port)
{
}

const std::string& Broker::get_host() const
{
    return m_host;
}

std::uint16_t Broker::get_port() const
{
    return m_port;
}

std::string& Broker::get_host()
{
    return m_host;
}

StreamMetadata::StreamMetadata(std::string stream_name, ResponseCode response_code, Broker leader, std::vector<Broker> replicas)
 : m_stream_name(std::move(stream_name)),
   m_response_code(response_code),
   m_leader(std::move(leader)),
   m_replicas(std::move(replicas))
{
}

const std::string& StreamMetadata::get_stream_name() const
{
    return m_stream_name;
}

ResponseCode StreamMetadata::get_response_code() const
{
    return m_response_code;
}

const Broker& StreamMetadata::get_leader() const
{
    return m_leader;
}

const std::vector<Broker>& StreamMetadata::get_replicas() const
{
    return m_replicas;
}

std::string& StreamMetadata::get_stream_name()
{
    return m_stream_name;
}

Broker& StreamMetadata::get_leader()
{
    return m_leader;
}

std::vector<Broker>& StreamMetadata::get_replicas()
{
    return m_replicas;
}

}  // namespace hareflow