#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <optional>

#include "hareflow/client_parameters.h"
#include "hareflow/types.h"

namespace hareflow {

class HAREFLOW_EXPORT Client
{
public:
    static ClientPtr create(ClientParameters parameters);
    virtual ~Client() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;

    virtual StreamMetadataSet            metadata(const std::vector<std::string>& streams)                                                           = 0;
    virtual void                         declare_publisher(std::uint8_t publisher_id, std::string_view publisher_reference, std::string_view stream) = 0;
    virtual void                         delete_publisher(std::uint8_t publisher_id)                                                                 = 0;
    virtual std::optional<std::uint64_t> query_publisher_sequence(std::string_view publisher_reference, std::string_view stream)                     = 0;
    virtual void                         store_offset(std::string_view reference, std::string_view stream, std::uint64_t offset)                     = 0;
    virtual std::optional<std::uint64_t> query_offset(std::string_view reference, std::string_view stream)                                           = 0;
    virtual void                         create_stream(std::string_view stream, const Properties& arguments)                                         = 0;
    virtual void                         delete_stream(std::string_view stream)                                                                      = 0;
    virtual void                         subscribe(std::uint8_t               subscription_id,
                                                   std::string_view           stream,
                                                   const OffsetSpecification& offset,
                                                   std::uint16_t              credit,
                                                   const Properties&          properties)                                                                     = 0;
    virtual void                         unsubscribe(std::uint8_t subscription_id)                                                                   = 0;
    virtual void                         credit(std::uint8_t subscription_id, std::uint16_t credit)                                                  = 0;
    virtual void                         publish(std::uint8_t publisher_id, std::uint64_t publishing_id, MessagePtr message)                         = 0;
};

}  // namespace hareflow