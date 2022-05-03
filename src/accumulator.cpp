#include "hareflow/detail/accumulator.h"

#include <fmt/core.h>

#include "hareflow/codec.h"
#include "hareflow/detail/commands.h"

namespace hareflow::detail {

Accumulator::Accumulator(std::uint32_t capacity, CodecPtr codec)
 : m_capacity(capacity),
   m_codec(std::move(codec)),
   m_mutex(),
   m_max_frame_size(0),
   m_space_available(),
   m_pending(),
   m_destroyed(false)
{
    m_pending.reserve(m_capacity);
}

bool Accumulator::add(std::uint64_t publishing_id, MessagePtr message, ConfirmationHandler confirmation_handler)
{
    std::vector<std::uint8_t> encoded = m_codec->encode(message);

    std::unique_lock lock{m_mutex};
    if (m_max_frame_size > 0 &&
        PublishCommand::BASE_SERIALIZED_SIZE + PublishCommand::Message{publishing_id, boost::asio::buffer(encoded)}.serialized_size() > m_max_frame_size) {
        throw InvalidInputException{fmt::format("Maximum frame size exceeded ({})", m_max_frame_size)};
    }

    AccumulatedMessagePtr accumulated = std::make_shared<AccumulatedMessage>(publishing_id,
                                                                             std::chrono::steady_clock::now(),
                                                                             std::move(message),
                                                                             std::move(encoded),
                                                                             std::move(confirmation_handler));
    m_space_available.wait(lock, [this] { return m_pending.size() < m_capacity || m_destroyed; });
    if (m_destroyed) {
        throw AccumulatorDestroyedException{"Accumulator was destroyed"};
    }
    m_pending.emplace_back(std::move(accumulated));
    return m_pending.size() == m_capacity;
}

std::vector<AccumulatedMessagePtr> Accumulator::extract_all()
{
    std::vector<AccumulatedMessagePtr> extracted;
    extracted.reserve(m_capacity);
    {
        std::unique_lock lock{m_mutex};
        using std::swap;
        swap(m_pending, extracted);
    }
    m_space_available.notify_all();
    return extracted;
}

void Accumulator::set_max_frame_size(std::uint32_t max_frame_size)
{
    std::unique_lock lock{m_mutex};
    m_max_frame_size = max_frame_size;
}

void Accumulator::destroy()
{
    {
        std::unique_lock lock{m_mutex};
        m_destroyed = true;
    }
    m_space_available.notify_all();
}

}  // namespace hareflow::detail