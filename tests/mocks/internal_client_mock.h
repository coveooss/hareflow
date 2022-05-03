#include <gmock/gmock.h>

#include "hareflow/detail/client_impl.h"

namespace hareflow::tests {

class InternalClientMock : public detail::InternalClient
{
public:
    MOCK_METHOD(std::uint32_t, max_frame_size, (), (const, override));
    MOCK_METHOD(void, publish_encoded, (std::uint8_t publisher_id, const std::vector<detail::PublishCommand::Message>& messages), (override));

    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));

    MOCK_METHOD(StreamMetadataSet, metadata, (const std::vector<std::string>& streams), (override));
    MOCK_METHOD(void, declare_publisher, (std::uint8_t publisher_id, std::string_view publisher_reference, std::string_view stream), (override));
    MOCK_METHOD(void, delete_publisher, (std::uint8_t publisher_id), (override));
    MOCK_METHOD(std::optional<std::uint64_t>, query_publisher_sequence, (std::string_view publisher_reference, std::string_view stream), (override));
    MOCK_METHOD(void, store_offset, (std::string_view reference, std::string_view stream, std::uint64_t offset), (override));
    MOCK_METHOD(std::optional<std::uint64_t>, query_offset, (std::string_view reference, std::string_view stream), (override));
    MOCK_METHOD(void, create_stream, (std::string_view stream, const Properties& arguments), (override));
    MOCK_METHOD(void, delete_stream, (std::string_view stream), (override));
    MOCK_METHOD(void,
                subscribe,
                (std::uint8_t subscription_id, std::string_view stream, const OffsetSpecification& offset, std::uint16_t credit, const Properties& properties),
                (override));
    MOCK_METHOD(void, unsubscribe, (std::uint8_t subscription_id), (override));
    MOCK_METHOD(void, credit, (std::uint8_t subscription_id, std::uint16_t credit), (override));
    MOCK_METHOD(void, publish, (std::uint8_t publisher_id, std::uint64_t publishing_id, MessagePtr message), (override));
};

}  // namespace hareflow::tests