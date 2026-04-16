#pragma once

#include "mfms/protocol/frame.hpp"
#include "mfms/common/types.hpp"
#include <vector>
#include <cstddef>

namespace mfms::protocol {

// Incremental frame parser that handles partial reads
class FrameParser {
public:
    explicit FrameParser(std::size_t max_payload = kMaxPayloadLen);

    // Feed data to parser. Returns number of bytes consumed.
    std::size_t feed(const std::uint8_t* data, std::size_t len);

    // Check if a complete frame is available
    bool hasFrame() const { return state_ == State::COMPLETE; }

    // Get the parsed header (valid only when hasFrame() == true)
    const FrameHeader& header() const { return header_; }

    // Get the payload buffer (valid only when hasFrame() == true)
    const std::vector<std::uint8_t>& payload() const { return payload_; }

    // Move payload out (transfers ownership, resets parser)
    std::vector<std::uint8_t> takePayload();

    // Reset parser state (call after processing a frame)
    void reset();

    // Get last error (if state is ERROR)
    ErrorCode lastError() const { return last_error_; }

    // Check if parser is in error state
    bool hasError() const { return state_ == State::ERROR; }

private:
    enum class State {
        READING_HEADER,
        READING_PAYLOAD,
        COMPLETE,
        ERROR,
    };

    State state_{State::READING_HEADER};
    std::size_t max_payload_;
    std::size_t bytes_needed_{kHeaderSize};
    std::size_t buffer_offset_{0};

    std::uint8_t header_buf_[kHeaderSize]{};
    FrameHeader header_{};
    std::vector<std::uint8_t> payload_;
    ErrorCode last_error_{ErrorCode::OK};
};

} // namespace mfms::protocol
