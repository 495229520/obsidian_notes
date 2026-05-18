#pragma once

#include "mfms/protocol/frame.hpp"
#include "mfms/common/types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace mfms::protocol {

// Maximum file size for uploads (30 MB)
inline constexpr std::uint32_t kMaxUploadSize = 30u * 1024u * 1024u;

// ============ Upload Payloads ============

// Upload BEGIN payload (sent by client)
struct UploadBeginPayload {
    UploadId upload_id{0};          // Server-generated or client-provided
    std::uint32_t total_size{0};    // Expected total file size
    std::uint32_t chunk_size{0};    // Proposed chunk size
    std::uint32_t file_crc32{0};    // CRC32 of complete file (0 = unused)
    std::string filename;           // Sanitized filename
    std::string target_robot_id;    // For LUA uploads, target robot; empty for QT logs

    bool serialize(std::vector<std::uint8_t>& out) const;
    static bool deserialize(const std::uint8_t* data, std::size_t len, UploadBeginPayload& out);
};

// Upload CHUNK payload
struct UploadChunkPayload {
    UploadId upload_id{0};
    std::uint32_t offset{0};        // Must equal received_size (sequential)
    std::uint32_t chunk_crc32{0};   // CRC32 of chunk data (0 = unused)
    std::vector<std::uint8_t> data;

    bool serialize(std::vector<std::uint8_t>& out) const;
    static bool deserialize(const std::uint8_t* data, std::size_t len, UploadChunkPayload& out);
};

// Upload END payload
struct UploadEndPayload {
    UploadId upload_id{0};
    std::uint32_t final_size{0};    // Must match total_size from BEGIN
    std::uint32_t final_crc32{0};   // CRC32 of complete file (0 = unused)

    bool serialize(std::vector<std::uint8_t>& out) const;
    static bool deserialize(const std::uint8_t* data, std::size_t len, UploadEndPayload& out);
};

// ============ Motion Request/Response ============

struct MotionRequestPayload {
    std::string robot_id;
    std::string command;            // JSON command string
    std::uint32_t timeout_ms{5000}; // Timeout for service call

    bool serialize(std::vector<std::uint8_t>& out) const;
    static bool deserialize(const std::uint8_t* data, std::size_t len, MotionRequestPayload& out);
};

struct MotionResponsePayload {
    std::string robot_id;
    bool success{false};
    int error_code{0};
    std::string result;             // JSON result string

    bool serialize(std::vector<std::uint8_t>& out) const;
    static bool deserialize(const std::uint8_t* data, std::size_t len, MotionResponsePayload& out);
};

// ============ Error Payload ============

struct ErrorPayload {
    std::uint16_t code{0};
    std::string detail;

    bool serialize(std::vector<std::uint8_t>& out) const;
    static bool deserialize(const std::uint8_t* data, std::size_t len, ErrorPayload& out);
};

// ============ ACK Payload ============

struct AckPayload {
    std::uint16_t status{0};        // 0 = OK
    std::string detail;             // Optional

    bool serialize(std::vector<std::uint8_t>& out) const;
    static bool deserialize(const std::uint8_t* data, std::size_t len, AckPayload& out);
};

// ============ Server Log Request ============

struct ServerLogRequestPayload {
    std::string date;               // YYYY-MM-DD
    std::uint32_t offset{0};        // Byte offset to start from
    std::uint32_t max_size{0};      // Max bytes to return (0 = all)

    bool serialize(std::vector<std::uint8_t>& out) const;
    static bool deserialize(const std::uint8_t* data, std::size_t len, ServerLogRequestPayload& out);
};

// ============ Utility ============

// Validate and sanitize filename (prevent path traversal)
bool sanitizeFilename(const std::string& input, std::string& output);

} // namespace mfms::protocol
