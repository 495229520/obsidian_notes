#pragma once

#include "mfms/common/types.hpp"
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <cstdint>

namespace mfms::codec {

// Normalized status value types
using StatusValue = std::variant<
    std::nullptr_t,
    bool,
    std::int64_t,
    double,
    std::string,
    std::vector<std::uint8_t>  // Raw binary
>;

// Normalized status map
using NormalizedStatus = std::unordered_map<std::string, StatusValue>;

// Robot status envelope
struct RobotStatusEnvelope {
    std::string robot_id;
    std::string robot_type;
    std::int64_t timestamp_ns{0};
    std::vector<std::uint8_t> raw_payload;
};

// Codec interface
class IRobotCodec {
public:
    virtual ~IRobotCodec() = default;

    // Decode raw payload to normalized status
    virtual Result<NormalizedStatus> decode(const std::vector<std::uint8_t>& raw) const = 0;

    // Get codec name/type
    virtual const std::string& typeName() const = 0;
};

// Generic JSON codec (if payload is already JSON)
class GenericJsonCodec : public IRobotCodec {
public:
    Result<NormalizedStatus> decode(const std::vector<std::uint8_t>& raw) const override;
    const std::string& typeName() const override {
        static const std::string name = "json";
        return name;
    }
};

// Fallback codec (stores raw as base64)
class FallbackCodec : public IRobotCodec {
public:
    Result<NormalizedStatus> decode(const std::vector<std::uint8_t>& raw) const override;
    const std::string& typeName() const override {
        static const std::string name = "fallback";
        return name;
    }
};

// Codec registry
class CodecRegistry {
public:
    CodecRegistry();

    // Register a codec for a robot type
    void registerCodec(const std::string& robot_type, std::shared_ptr<IRobotCodec> codec);

    // Get codec for robot type (returns fallback if not found)
    IRobotCodec* getCodec(const std::string& robot_type) const;

    // Decode using appropriate codec
    Result<NormalizedStatus> decode(const RobotStatusEnvelope& envelope) const;

private:
    std::unordered_map<std::string, std::shared_ptr<IRobotCodec>> codecs_;
    std::shared_ptr<IRobotCodec> json_codec_;
    std::shared_ptr<IRobotCodec> fallback_codec_;
};

// Utility: convert NormalizedStatus to JSON string
std::string normalizedStatusToJson(const NormalizedStatus& status);

// Utility: base64 encode
std::string base64Encode(const std::vector<std::uint8_t>& data);

} // namespace mfms::codec
