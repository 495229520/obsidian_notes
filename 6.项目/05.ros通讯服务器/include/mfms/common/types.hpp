#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <chrono>
#include <functional>
#include <memory>

namespace mfms {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;
using RequestId = std::uint32_t;
using ClientId = std::uint64_t;
using UploadId = std::uint64_t;

// Result type for operations
template<typename T>
struct Result {
    bool success{false};
    int code{0};
    std::string message;
    T data{};

    static Result<T> ok(T&& val) {
        return {true, 0, "", std::move(val)};
    }
    static Result<T> ok(const T& val) {
        return {true, 0, "", val};
    }
    static Result<T> err(int code, std::string msg) {
        return {false, code, std::move(msg), T{}};
    }
};

template<>
struct Result<void> {
    bool success{false};
    int code{0};
    std::string message;

    static Result<void> ok() {
        return {true, 0, ""};
    }
    static Result<void> err(int code, std::string msg) {
        return {false, code, std::move(msg)};
    }
};

// Error codes
enum class ErrorCode : int {
    OK = 0,
    INVALID_FRAME = 1001,
    PAYLOAD_TOO_LARGE = 1002,
    INVALID_MAGIC = 1003,
    UNSUPPORTED_VERSION = 1004,
    UNKNOWN_MSG_TYPE = 1005,
    CRC_MISMATCH = 1006,

    UPLOAD_ALREADY_IN_PROGRESS = 2001,
    UPLOAD_NOT_STARTED = 2002,
    UPLOAD_CHUNK_OUT_OF_ORDER = 2003,
    UPLOAD_SIZE_MISMATCH = 2004,
    UPLOAD_TIMEOUT = 2005,
    UPLOAD_TOO_LARGE = 2006,
    INVALID_FILENAME = 2007,

    ROBOT_NOT_FOUND = 3001,
    SERVICE_TIMEOUT = 3002,
    SERVICE_FAILED = 3003,

    DB_ERROR = 4001,
    DB_TIMEOUT = 4002,

    BACKPRESSURE = 5001,
    TOO_MANY_REQUESTS = 5002,
    IDLE_TIMEOUT = 5003,

    INTERNAL_ERROR = 9999,
};

} // namespace mfms
