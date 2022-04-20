#pragma once

#include <cstddef>
#include <cstdint>

#include <string>

#include <boost/asio.hpp>
#include <fmt/core.h>

#include "hareflow/exceptions.h"
#include "hareflow/types.h"

namespace hareflow::detail {

class BinaryBuffer
{
public:
    static std::size_t                                                 serialized_size(std::string_view string);
    static std::size_t                                                 serialized_size(boost::asio::const_buffer blob);
    template<typename Container, typename ElemSize> static std::size_t serialized_size(Container&& container, ElemSize&& elem_size);

    BinaryBuffer()                    = default;
    BinaryBuffer(const BinaryBuffer&) = delete;
    BinaryBuffer(BinaryBuffer&&)      = default;
    BinaryBuffer& operator=(const BinaryBuffer&) = delete;
    BinaryBuffer& operator=(BinaryBuffer&&) = default;

    std::size_t size() const;
    void        reserve(std::size_t size);
    void        resize(std::size_t size);
    void        clear();
    void        reset_cursors();
    std::size_t read_cursor() const;
    void        read_cursor(std::size_t cursor);
    std::size_t write_cursor() const;
    void        write_cursor(std::size_t cursor);

    boost::asio::mutable_buffer as_asio_buffer();

    std::uint8_t              read_byte();
    std::uint16_t             read_ushort();
    std::uint32_t             read_uint();
    std::uint64_t             read_ulong();
    std::int16_t              read_short();
    std::int32_t              read_int();
    std::int64_t              read_long();
    std::string_view          read_string();
    boost::asio::const_buffer read_blob();
    boost::asio::const_buffer read_bytes(std::uint32_t count);

    void write_byte(std::uint8_t val);
    void write_ushort(std::uint16_t val);
    void write_uint(std::uint32_t val);
    void write_ulong(std::uint64_t val);
    void write_short(std::int16_t val);
    void write_int(std::int32_t val);
    void write_long(std::int64_t val);
    void write_string(std::string_view string);
    void write_blob(boost::asio::const_buffer blob);
    void write_bytes(boost::asio::const_buffer bytes);

    template<typename Container, typename ReadElem> void  read_array(Container&& container, ReadElem&& read_elem);
    template<typename Container, typename WriteElem> void write_array(Container&& container, WriteElem&& write_elem);

private:
    std::vector<std::uint8_t> m_bytes{};
    std::size_t               m_read_cursor{0};
    std::size_t               m_write_cursor{0};

    void check_read_length(std::size_t to_read);
    void grow_buffer(std::size_t to_add);
};

}  // namespace hareflow::detail

#include "binary_buffer.hpp"