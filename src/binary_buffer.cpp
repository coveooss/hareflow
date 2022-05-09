#include "hareflow/detail/binary_buffer.h"

#include <boost/endian/conversion.hpp>

namespace hareflow::detail {

std::size_t BinaryBuffer::serialized_size(std::string_view string)
{
    return sizeof(std::int16_t) + string.size();
}

std::size_t BinaryBuffer::serialized_size(boost::asio::const_buffer blob)
{
    return sizeof(std::int32_t) + blob.size();
}

std::size_t BinaryBuffer::size() const
{
    return m_bytes.size();
}

void BinaryBuffer::reserve(std::size_t size)
{
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        throw InvalidInputException(fmt::format("Maximum buffer size exceeded ({})", std::numeric_limits<std::uint32_t>::max()));
    }
    m_bytes.reserve(size);
}

void BinaryBuffer::resize(std::size_t size)
{
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        throw InvalidInputException(fmt::format("Maximum buffer size exceeded ({})", std::numeric_limits<std::uint32_t>::max()));
    }
    m_bytes.resize(size);
}

void BinaryBuffer::clear()
{
    m_bytes.clear();
    reset_cursors();
}

void BinaryBuffer::reset_cursors()
{
    m_read_cursor  = 0;
    m_write_cursor = 0;
}

std::size_t BinaryBuffer::read_cursor() const
{
    return m_read_cursor;
}

void BinaryBuffer::read_cursor(std::size_t cursor)
{
    if (cursor > m_bytes.size()) {
        throw InvalidInputException("Read cursor out of range");
    }
    m_read_cursor = cursor;
}

std::size_t BinaryBuffer::write_cursor() const
{
    return m_write_cursor;
}

void BinaryBuffer::write_cursor(std::size_t cursor)
{
    if (cursor > m_bytes.size()) {
        throw InvalidInputException("Write cursor out of range");
    }
    m_write_cursor = cursor;
}

boost::asio::mutable_buffer BinaryBuffer::as_asio_buffer()
{
    return boost::asio::mutable_buffer(m_bytes.data(), m_bytes.size());
}

std::uint8_t BinaryBuffer::read_byte()
{
    check_read_length(sizeof(std::uint8_t));
    std::uint8_t val = m_bytes[m_read_cursor];
    m_read_cursor += sizeof(std::uint8_t);
    return val;
}

std::uint16_t BinaryBuffer::read_ushort()
{
    check_read_length(sizeof(std::uint16_t));
    std::uint16_t val;
    memcpy(&val, &m_bytes[m_read_cursor], sizeof(std::uint16_t));
    boost::endian::big_to_native_inplace(val);
    m_read_cursor += sizeof(std::uint16_t);
    return val;
}

std::uint32_t BinaryBuffer::read_uint()
{
    check_read_length(sizeof(std::uint32_t));
    std::uint32_t val;
    memcpy(&val, &m_bytes[m_read_cursor], sizeof(std::uint32_t));
    boost::endian::big_to_native_inplace(val);
    m_read_cursor += sizeof(std::uint32_t);
    return val;
}

std::uint64_t BinaryBuffer::read_ulong()
{
    check_read_length(sizeof(std::uint64_t));
    std::uint64_t val;
    memcpy(&val, &m_bytes[m_read_cursor], sizeof(std::uint64_t));
    boost::endian::big_to_native_inplace(val);
    m_read_cursor += sizeof(std::uint64_t);
    return val;
}

std::int16_t BinaryBuffer::read_short()
{
    check_read_length(sizeof(std::int16_t));
    std::int16_t val;
    memcpy(&val, &m_bytes[m_read_cursor], sizeof(std::int16_t));
    boost::endian::big_to_native_inplace(val);
    m_read_cursor += sizeof(std::int16_t);
    return val;
}

std::int32_t BinaryBuffer::read_int()
{
    check_read_length(sizeof(std::int32_t));
    std::int32_t val;
    memcpy(&val, &m_bytes[m_read_cursor], sizeof(std::int32_t));
    boost::endian::big_to_native_inplace(val);
    m_read_cursor += sizeof(std::int32_t);
    return val;
}

std::int64_t BinaryBuffer::read_long()
{
    check_read_length(sizeof(std::int64_t));
    std::int64_t val;
    memcpy(&val, &m_bytes[m_read_cursor], sizeof(std::int64_t));
    boost::endian::big_to_native_inplace(val);
    m_read_cursor += sizeof(std::int64_t);
    return val;
}

std::string_view BinaryBuffer::read_string()
{
    std::int16_t length = read_short();
    if (length <= 0) {
        return std::string_view();
    }
    check_read_length(length);
    std::string_view string(reinterpret_cast<char*>(&m_bytes[m_read_cursor]), length);
    m_read_cursor += length;
    return string;
}

boost::asio::const_buffer BinaryBuffer::read_blob()
{
    std::int32_t length = read_int();
    if (length <= 0) {
        return boost::asio::const_buffer();
    }
    return read_bytes(length);
}

boost::asio::const_buffer BinaryBuffer::read_bytes(std::uint32_t count)
{
    if (count == 0) {
        return boost::asio::const_buffer();
    }
    check_read_length(count);
    auto bytes = boost::asio::const_buffer(&m_bytes[m_read_cursor], count);
    m_read_cursor += count;
    return bytes;
}

void BinaryBuffer::write_byte(std::uint8_t byte)
{
    grow_buffer(sizeof(std::uint8_t));
    m_bytes[m_write_cursor] = byte;
    m_write_cursor += sizeof(byte);
}

void BinaryBuffer::write_ushort(std::uint16_t val)
{
    grow_buffer(sizeof(std::uint16_t));
    boost::endian::native_to_big_inplace(val);
    memcpy(&m_bytes[m_write_cursor], &val, sizeof(std::uint16_t));
    m_write_cursor += sizeof(std::uint16_t);
}

void BinaryBuffer::write_uint(std::uint32_t val)
{
    grow_buffer(sizeof(std::uint32_t));
    boost::endian::native_to_big_inplace(val);
    memcpy(&m_bytes[m_write_cursor], &val, sizeof(std::uint32_t));
    m_write_cursor += sizeof(std::uint32_t);
}

void BinaryBuffer::write_ulong(std::uint64_t val)
{
    grow_buffer(sizeof(std::uint64_t));
    boost::endian::native_to_big_inplace(val);
    memcpy(&m_bytes[m_write_cursor], &val, sizeof(std::uint64_t));
    m_write_cursor += sizeof(std::uint64_t);
}

void BinaryBuffer::write_short(std::int16_t val)
{
    grow_buffer(sizeof(std::int16_t));
    boost::endian::native_to_big_inplace(val);
    memcpy(&m_bytes[m_write_cursor], &val, sizeof(std::int16_t));
    m_write_cursor += sizeof(std::int16_t);
}

void BinaryBuffer::write_int(std::int32_t val)
{
    grow_buffer(sizeof(std::int32_t));
    boost::endian::native_to_big_inplace(val);
    memcpy(&m_bytes[m_write_cursor], &val, sizeof(std::int32_t));
    m_write_cursor += sizeof(std::int32_t);
}

void BinaryBuffer::write_long(std::int64_t val)
{
    grow_buffer(sizeof(std::int64_t));
    boost::endian::native_to_big_inplace(val);
    memcpy(&m_bytes[m_write_cursor], &val, sizeof(std::int64_t));
    m_write_cursor += sizeof(std::int64_t);
}

void BinaryBuffer::write_string(std::string_view string)
{
    static constexpr std::size_t max_as_size_t = std::numeric_limits<std::int16_t>::max();
    if (string.size() > max_as_size_t) {
        throw InvalidInputException(fmt::format("Maximum string size exceeded ({})", max_as_size_t));
    }
    std::int16_t length = static_cast<std::int16_t>(string.size());
    write_short(length);
    if (length == 0) {
        return;
    }
    write_bytes(boost::asio::buffer(string));
}

void BinaryBuffer::write_blob(boost::asio::const_buffer blob)
{
    static constexpr std::size_t max_as_size_t = std::numeric_limits<std::int32_t>::max();
    if (blob.size() > max_as_size_t) {
        throw InvalidInputException(fmt::format("Maximum blob size exceeded ({})", max_as_size_t));
    }
    std::int32_t length = static_cast<std::int32_t>(blob.size());
    write_int(length);
    if (length == 0) {
        return;
    }
    write_bytes(blob);
}

void BinaryBuffer::write_bytes(boost::asio::const_buffer bytes)
{
    std::size_t length = bytes.size();
    if (length == 0) {
        return;
    }
    grow_buffer(length);
    memcpy(&m_bytes[m_write_cursor], bytes.data(), length);
    m_write_cursor += length;
}

void BinaryBuffer::check_read_length(std::size_t to_read)
{
    if (m_bytes.size() < m_read_cursor + to_read) {
        throw InvalidInputException("Not enough data left in buffer");
    }
}

void BinaryBuffer::grow_buffer(std::size_t to_add)
{
    if (std::numeric_limits<std::uint32_t>::max() - m_write_cursor < to_add) {
        throw InvalidInputException(fmt::format("Maximum buffer size exceeded ({})", std::numeric_limits<std::uint32_t>::max()));
    }
    m_bytes.resize(m_write_cursor + to_add);
}

}  // namespace hareflow::detail