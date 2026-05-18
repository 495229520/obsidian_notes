#include "mfms/protocol/messages.hpp"
#include <algorithm>
#include <cctype>

namespace mfms::protocol {

// ============ Serialization Helpers ============

static void appendString(std::vector<std::uint8_t>& out, const std::string& s) {
    std::uint16_t len = static_cast<uint16_t>(std::min(s.size(), size_t(65535)));
    out.push_back(static_cast<uint8_t>(len));
    out.push_back(static_cast<uint8_t>(len >> 8));
    out.insert(out.end(), s.begin(), s.begin() + len);
}

static bool readString(const std::uint8_t*& p, std::size_t& remaining, std::string& out) {
    if (remaining < 2) return false;
    std::uint16_t len = readU16LE(p);
    p += 2;
    remaining -= 2;
    if (remaining < len) return false;
    out.assign(reinterpret_cast<const char*>(p), len);
    p += len;
    remaining -= len;
    return true;
}

// ============ UploadBeginPayload ============

bool UploadBeginPayload::serialize(std::vector<std::uint8_t>& out) const {
    out.reserve(out.size() + 24 + filename.size() + target_robot_id.size());
    std::uint8_t buf[8];
    writeU64LE(buf, upload_id);
    out.insert(out.end(), buf, buf + 8);
    writeU32LE(buf, total_size);
    out.insert(out.end(), buf, buf + 4);
    writeU32LE(buf, chunk_size);
    out.insert(out.end(), buf, buf + 4);
    writeU32LE(buf, file_crc32);
    out.insert(out.end(), buf, buf + 4);
    appendString(out, filename);
    appendString(out, target_robot_id);
    return true;
}

bool UploadBeginPayload::deserialize(const std::uint8_t* data, std::size_t len, UploadBeginPayload& out) {
    if (len < 20) return false;
    const std::uint8_t* p = data;
    std::size_t remaining = len;

    out.upload_id = readU64LE(p); p += 8; remaining -= 8;
    out.total_size = readU32LE(p); p += 4; remaining -= 4;
    out.chunk_size = readU32LE(p); p += 4; remaining -= 4;
    out.file_crc32 = readU32LE(p); p += 4; remaining -= 4;

    if (!readString(p, remaining, out.filename)) return false;
    if (!readString(p, remaining, out.target_robot_id)) return false;

    return true;
}

// ============ UploadChunkPayload ============

bool UploadChunkPayload::serialize(std::vector<std::uint8_t>& out) const {
    out.reserve(out.size() + 20 + data.size());
    std::uint8_t buf[8];
    writeU64LE(buf, upload_id);
    out.insert(out.end(), buf, buf + 8);
    writeU32LE(buf, offset);
    out.insert(out.end(), buf, buf + 4);
    writeU32LE(buf, chunk_crc32);
    out.insert(out.end(), buf, buf + 4);
    writeU32LE(buf, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), buf, buf + 4);
    out.insert(out.end(), data.begin(), data.end());
    return true;
}

bool UploadChunkPayload::deserialize(const std::uint8_t* data_in, std::size_t len, UploadChunkPayload& out) {
    if (len < 20) return false;
    const std::uint8_t* p = data_in;

    out.upload_id = readU64LE(p); p += 8;
    out.offset = readU32LE(p); p += 4;
    out.chunk_crc32 = readU32LE(p); p += 4;
    std::uint32_t data_len = readU32LE(p); p += 4;

    if (len < 20 + data_len) return false;
    out.data.assign(p, p + data_len);
    return true;
}

// ============ UploadEndPayload ============

bool UploadEndPayload::serialize(std::vector<std::uint8_t>& out) const {
    std::uint8_t buf[16];
    writeU64LE(buf, upload_id);
    writeU32LE(buf + 8, final_size);
    writeU32LE(buf + 12, final_crc32);
    out.insert(out.end(), buf, buf + 16);
    return true;
}

bool UploadEndPayload::deserialize(const std::uint8_t* data, std::size_t len, UploadEndPayload& out) {
    if (len < 16) return false;
    out.upload_id = readU64LE(data);
    out.final_size = readU32LE(data + 8);
    out.final_crc32 = readU32LE(data + 12);
    return true;
}

// ============ MotionRequestPayload ============

bool MotionRequestPayload::serialize(std::vector<std::uint8_t>& out) const {
    appendString(out, robot_id);
    appendString(out, command);
    std::uint8_t buf[4];
    writeU32LE(buf, timeout_ms);
    out.insert(out.end(), buf, buf + 4);
    return true;
}

bool MotionRequestPayload::deserialize(const std::uint8_t* data, std::size_t len, MotionRequestPayload& out) {
    const std::uint8_t* p = data;
    std::size_t remaining = len;

    if (!readString(p, remaining, out.robot_id)) return false;
    if (!readString(p, remaining, out.command)) return false;
    if (remaining < 4) return false;
    out.timeout_ms = readU32LE(p);
    return true;
}

// ============ MotionResponsePayload ============

bool MotionResponsePayload::serialize(std::vector<std::uint8_t>& out) const {
    appendString(out, robot_id);
    out.push_back(success ? 1 : 0);
    std::uint8_t buf[4];
    writeU32LE(buf, static_cast<uint32_t>(error_code));
    out.insert(out.end(), buf, buf + 4);
    appendString(out, result);
    return true;
}

bool MotionResponsePayload::deserialize(const std::uint8_t* data, std::size_t len, MotionResponsePayload& out) {
    const std::uint8_t* p = data;
    std::size_t remaining = len;

    if (!readString(p, remaining, out.robot_id)) return false;
    if (remaining < 5) return false;
    out.success = (*p++ != 0);
    remaining--;
    out.error_code = static_cast<int>(readU32LE(p));
    p += 4; remaining -= 4;
    if (!readString(p, remaining, out.result)) return false;
    return true;
}

// ============ ErrorPayload ============

bool ErrorPayload::serialize(std::vector<std::uint8_t>& out) const {
    std::uint8_t buf[2];
    writeU16LE(buf, code);
    out.insert(out.end(), buf, buf + 2);
    appendString(out, detail);
    return true;
}

bool ErrorPayload::deserialize(const std::uint8_t* data, std::size_t len, ErrorPayload& out) {
    if (len < 2) return false;
    const std::uint8_t* p = data;
    std::size_t remaining = len;

    out.code = readU16LE(p);
    p += 2; remaining -= 2;
    if (!readString(p, remaining, out.detail)) return false;
    return true;
}

// ============ AckPayload ============

bool AckPayload::serialize(std::vector<std::uint8_t>& out) const {
    std::uint8_t buf[2];
    writeU16LE(buf, status);
    out.insert(out.end(), buf, buf + 2);
    appendString(out, detail);
    return true;
}

bool AckPayload::deserialize(const std::uint8_t* data, std::size_t len, AckPayload& out) {
    if (len < 2) return false;
    const std::uint8_t* p = data;
    std::size_t remaining = len;

    out.status = readU16LE(p);
    p += 2; remaining -= 2;
    if (!readString(p, remaining, out.detail)) return false;
    return true;
}

// ============ ServerLogRequestPayload ============

bool ServerLogRequestPayload::serialize(std::vector<std::uint8_t>& out) const {
    appendString(out, date);
    std::uint8_t buf[8];
    writeU32LE(buf, offset);
    writeU32LE(buf + 4, max_size);
    out.insert(out.end(), buf, buf + 8);
    return true;
}

bool ServerLogRequestPayload::deserialize(const std::uint8_t* data, std::size_t len, ServerLogRequestPayload& out) {
    const std::uint8_t* p = data;
    std::size_t remaining = len;

    if (!readString(p, remaining, out.date)) return false;
    if (remaining < 8) return false;
    out.offset = readU32LE(p);
    out.max_size = readU32LE(p + 4);
    return true;
}

// ============ Filename Sanitization ============

bool sanitizeFilename(const std::string& input, std::string& output) {
    if (input.empty() || input.size() > 255) {
        return false;
    }

    output.clear();
    output.reserve(input.size());

    for (char c : input) {
        // Reject path separators and other dangerous characters
        if (c == '/' || c == '\\' || c == '\0' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            return false;
        }
        // Allow alphanumeric, dash, underscore, dot
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') {
            output += c;
        } else {
            output += '_'; // Replace other chars with underscore
        }
    }

    // Reject empty result or hidden files
    if (output.empty() || output[0] == '.') {
        return false;
    }

    // Reject reserved names
    if (output == "." || output == "..") {
        return false;
    }

    return true;
}

} // namespace mfms::protocol
