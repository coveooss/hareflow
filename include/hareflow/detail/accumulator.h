#pragma once

#include <cstdint>

#include <mutex>
#include <chrono>
#include <condition_variable>
#include <vector>

#include "hareflow/detail/internal_types.h"

namespace hareflow::detail {

struct AccumulatedMessage {
    std::uint64_t                         publishing_id;
    std::chrono::steady_clock::time_point publish_time;
    MessagePtr                            message;
    const std::vector<std::uint8_t>       encoded_message;
    ConfirmationHandler                   confirmation_handler;
};

class AccumulatorDestroyedException : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class Accumulator
{
public:
    Accumulator(std::uint32_t capacity, CodecPtr codec);

    bool                               add(std::uint64_t publishing_id, MessagePtr message, ConfirmationHandler confirmation_handler);
    std::vector<AccumulatedMessagePtr> extract_all();

    void set_max_frame_size(std::uint32_t max_frame_size);
    void destroy();

private:
    const std::uint32_t                m_capacity;
    const CodecPtr                     m_codec;
    std::mutex                         m_mutex;
    std::uint32_t                      m_max_frame_size;
    std::condition_variable            m_space_available;
    std::vector<AccumulatedMessagePtr> m_pending;
    bool                               m_destroyed;
};

}  // namespace hareflow::detail