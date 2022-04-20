#pragma once

#include <cstdint>

#include <map>
#include <string>
#include <vector>

#include "hareflow/types.h"

namespace hareflow {

class HAREFLOW_EXPORT MessageBuilder
{
public:
    MessageBuilder& body(std::vector<std::uint8_t> data) &;
    MessageBuilder& body(std::string_view data) &;
    MessageBuilder& body(const void* data, std::size_t data_size) &;
    MessageBuilder& headers(std::map<std::string, std::string> headers) &;

    MessageBuilder body(std::vector<std::uint8_t> data) &&;
    MessageBuilder body(std::string_view data) &&;
    MessageBuilder body(const void* data, std::size_t data_size) &&;
    MessageBuilder headers(std::map<std::string, std::string> headers) &&;

    MessagePtr build() const&;
    MessagePtr build() &&;

private:
    std::map<std::string, std::string> m_headers;
    std::vector<std::uint8_t>          m_body;
};

}  // namespace hareflow