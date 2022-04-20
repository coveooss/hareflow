#pragma once

#include <cstdint>

#include <chrono>
#include <string>

#include "hareflow/types.h"

namespace hareflow {

enum class LeaderLocator { LeastLeaders, ClientLocal, Random };

class StreamCreator
{
public:
    StreamCreator(detail::InternalEnvironmentPtr environment);
    HAREFLOW_EXPORT StreamCreator(const StreamCreator&) = default;
    HAREFLOW_EXPORT StreamCreator(StreamCreator&&)      = default;
    HAREFLOW_EXPORT ~StreamCreator()                    = default;
    HAREFLOW_EXPORT StreamCreator& operator=(const StreamCreator&) = default;
    HAREFLOW_EXPORT StreamCreator& operator=(StreamCreator&&) = default;

    HAREFLOW_EXPORT StreamCreator& stream(std::string stream) &;
    HAREFLOW_EXPORT StreamCreator& max_length(std::uint64_t bytes) &;
    HAREFLOW_EXPORT StreamCreator& max_segment_size(std::uint64_t bytes) &;
    HAREFLOW_EXPORT StreamCreator& max_age(std::chrono::seconds max_age) &;
    HAREFLOW_EXPORT StreamCreator& leader_locator(LeaderLocator leader_locator) &;

    HAREFLOW_EXPORT StreamCreator stream(std::string stream) &&;
    HAREFLOW_EXPORT StreamCreator max_length(std::uint64_t bytes) &&;
    HAREFLOW_EXPORT StreamCreator max_segment_size(std::uint64_t bytes) &&;
    HAREFLOW_EXPORT StreamCreator max_age(std::chrono::seconds max_age) &&;
    HAREFLOW_EXPORT StreamCreator leader_locator(LeaderLocator leader_locator) &&;

    HAREFLOW_EXPORT void create();

private:
    detail::InternalEnvironmentPtr m_environment;
    std::string                    m_stream;
    Properties                     m_properties;
};

}  // namespace hareflow