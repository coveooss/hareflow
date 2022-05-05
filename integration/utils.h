#pragma once

#include <random>
#include <string>

#include <gtest/gtest.h>

#include "hareflow.h"

namespace hareflow::tests {

inline std::string random_string(std::size_t length = 16)
{
    static constexpr std::string_view                              chars{"abcdefghijklmnopqrstuvwxyz"};
    static thread_local std::mt19937                               generator{std::random_device{}()};
    static thread_local std::uniform_int_distribution<std::size_t> distribution{0, chars.size() - 1};

    std::string result;
    result.resize(length);
    for (std::size_t i = 0; i < length; ++i) {
        result[i] = chars[distribution(generator)];
    }
    return result;
}

class TemporaryStream
{
public:
    TemporaryStream(EnvironmentPtr environment) : m_environment(environment), m_stream(random_string())
    {
        m_environment->stream_creator().stream(m_stream).create();
    }

    ~TemporaryStream()
    {
        EXPECT_NO_THROW(m_environment->delete_stream(m_stream));
    }

    const std::string& name()
    {
        return m_stream;
    }

private:
    EnvironmentPtr m_environment;
    std::string    m_stream;
};

}  // namespace hareflow::tests