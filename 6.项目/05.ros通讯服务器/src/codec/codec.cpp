#include "mfms/codec/codec.hpp"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <type_traits>
#include <variant>

namespace mfms::codec {

namespace {

const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace

std::string base64Encode(const std::vector<std::uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    std::size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        result += kBase64Chars[(triple >> 18) & 0x3F];
        result += kBase64Chars[(triple >> 12) & 0x3F];
        result += (i > data.size() + 1) ? '=' : kBase64Chars[(triple >> 6) & 0x3F];
        result += (i > data.size()) ? '=' : kBase64Chars[triple & 0x3F];
    }

    return result;
}

Result<NormalizedStatus> GenericJsonCodec::decode(const std::vector<std::uint8_t>& raw) const {
    try {
        auto json = nlohmann::json::parse(raw.begin(), raw.end());
        NormalizedStatus status;

        for (auto it = json.begin(); it != json.end(); ++it) {
            const std::string& key = it.key();
            const auto& value = it.value();
            if (value.is_null()) {
                status[key] = nullptr;
            } else if (value.is_boolean()) {
                status[key] = value.get<bool>();
            } else if (value.is_number_integer()) {
                status[key] = value.get<std::int64_t>();
            } else if (value.is_number_float()) {
                status[key] = value.get<double>();
            } else if (value.is_string()) {
                status[key] = value.get<std::string>();
            } else {
                // Nested objects/arrays: serialize back to string
                status[key] = value.dump();
            }
        }

        return Result<NormalizedStatus>::ok(std::move(status));
    } catch (const std::exception& e) {
        return Result<NormalizedStatus>::err(
            static_cast<int>(ErrorCode::INTERNAL_ERROR),
            std::string("JSON parse error: ") + e.what()
        );
    }
}

Result<NormalizedStatus> FallbackCodec::decode(const std::vector<std::uint8_t>& raw) const {
    NormalizedStatus status;
    status["_raw_base64"] = base64Encode(raw);
    status["_raw_size"] = static_cast<std::int64_t>(raw.size());
    return Result<NormalizedStatus>::ok(std::move(status));
}

CodecRegistry::CodecRegistry()
    : json_codec_(std::make_shared<GenericJsonCodec>())
    , fallback_codec_(std::make_shared<FallbackCodec>())
{}

void CodecRegistry::registerCodec(const std::string& robot_type, std::shared_ptr<IRobotCodec> codec) {
    codecs_[robot_type] = std::move(codec);
}

IRobotCodec* CodecRegistry::getCodec(const std::string& robot_type) const {
    auto it = codecs_.find(robot_type);
    if (it != codecs_.end()) {
        return it->second.get();
    }
    // Default: try JSON codec, with fallback as last resort
    return json_codec_.get();
}

Result<NormalizedStatus> CodecRegistry::decode(const RobotStatusEnvelope& envelope) const {
    auto* codec = getCodec(envelope.robot_type);
    auto result = codec->decode(envelope.raw_payload);

    if (result.success) {
        // Add metadata
        result.data["_robot_id"] = envelope.robot_id;
        result.data["_robot_type"] = envelope.robot_type;
        result.data["_timestamp_ns"] = envelope.timestamp_ns;
    } else {
        // Try fallback
        spdlog::warn("Codec {} failed for robot {}, using fallback",
                     codec->typeName(), envelope.robot_id);
        result = fallback_codec_->decode(envelope.raw_payload);
        if (result.success) {
            result.data["_robot_id"] = envelope.robot_id;
            result.data["_robot_type"] = envelope.robot_type;
            result.data["_timestamp_ns"] = envelope.timestamp_ns;
            result.data["_codec_fallback"] = true;
        }
    }

    return result;
}

std::string normalizedStatusToJson(const NormalizedStatus& status) {
    nlohmann::json json;

    for (const auto& kv : status) {
        const auto& key = kv.first;
        const auto& value = kv.second;
        std::visit([&json, &key](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                json[key] = nullptr;
            } else if constexpr (std::is_same_v<T, bool>) {
                json[key] = v;
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                json[key] = v;
            } else if constexpr (std::is_same_v<T, double>) {
                json[key] = v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                json[key] = v;
            } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
                json[key] = base64Encode(v);
            }
        }, value);
    }

    return json.dump();
}

} // namespace mfms::codec
