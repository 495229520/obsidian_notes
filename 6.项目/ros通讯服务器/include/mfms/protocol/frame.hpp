#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <optional>

namespace mfms::protocol {

// Wire format constants
inline constexpr std::size_t kHeaderSize = 24;
inline constexpr std::uint8_t kMagicBytes[4] = {0x4D, 0x46, 0x4D, 0x53}; // "MFMS"
inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::uint32_t kMaxPayloadLen = 32u * 1024u * 1024u; // 32 MiB

// Message types
enum class MsgType : std::uint16_t {
    // Client -> Server requests (0x00xx)
    STATUS_SNAPSHOT_REQ     = 0x0001,
    MOTION_REQ              = 0x0002,
    LUA_UPLOAD_BEGIN        = 0x0010,
    LUA_UPLOAD_CHUNK        = 0x0011,
    LUA_UPLOAD_END          = 0x0012,
    QT_LOG_BEGIN            = 0x0020,
    QT_LOG_CHUNK            = 0x0021,
    QT_LOG_END              = 0x0022,
    SERVER_LOG_REQ          = 0x0030,

    // Server -> Client responses (0x80xx)
    STATUS_SNAPSHOT_RESP    = 0x8001,
    MOTION_RESP             = 0x8002,
    LUA_FORWARD_RESULT      = 0x8012,
    SERVER_LOG_CHUNK        = 0x8030,
    SERVER_LOG_END          = 0x8031,

    // Generic responses (0xFFxx)
    ACK                     = 0xFF00,
    ERROR                   = 0xFFFF,
};

// Flags
enum class FrameFlags : std::uint32_t {
    NONE            = 0,
    REQUIRES_ACK    = 1 << 0,
    COMPRESSED      = 1 << 1,
    HAS_CRC         = 1 << 2,
};

inline FrameFlags operator|(FrameFlags a, FrameFlags b) {
    return static_cast<FrameFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool hasFlag(FrameFlags flags, FrameFlags test) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
}

// Frame header (logical structure - always serialize explicitly)
struct FrameHeader {
    std::uint16_t version{kProtocolVersion};
    MsgType msg_type{};
    FrameFlags flags{FrameFlags::NONE};
    std::uint32_t request_id{0};
    std::uint32_t payload_len{0};
    std::uint32_t header_crc32{0}; // CRC32 of bytes[0..19], 0 = disabled
};

// Byte order utilities (assuming little-endian wire format)
inline std::uint16_t readU16LE(const std::uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline std::uint32_t readU32LE(const std::uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline std::uint64_t readU64LE(const std::uint8_t* p) {
    return static_cast<uint64_t>(readU32LE(p)) |
           (static_cast<uint64_t>(readU32LE(p + 4)) << 32);
}

inline void writeU16LE(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}

inline void writeU32LE(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

inline void writeU64LE(std::uint8_t* p, std::uint64_t v) {
    writeU32LE(p, static_cast<uint32_t>(v));
    writeU32LE(p + 4, static_cast<uint32_t>(v >> 32));
}

// CRC32 (IEEE 802.3 polynomial)
std::uint32_t crc32(const std::uint8_t* data, std::size_t len);

// Serialize header to wire format (24 bytes)
void serializeHeader(std::uint8_t* out, const FrameHeader& hdr);

// Deserialize header from wire format
// Returns nullopt on invalid magic or unsupported version
std::optional<FrameHeader> deserializeHeader(const std::uint8_t* data);

// Validate header CRC (if HAS_CRC flag set)
bool validateHeaderCrc(const std::uint8_t* data, const FrameHeader& hdr);

} // namespace mfms::protocol
