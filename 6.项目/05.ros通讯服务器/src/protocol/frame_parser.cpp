#include "mfms/protocol/frame_parser.hpp"
#include <algorithm>
#include <cstring>

namespace mfms::protocol {

FrameParser::FrameParser(std::size_t max_payload)
    : max_payload_(max_payload) {}

std::size_t FrameParser::feed(const std::uint8_t* data, std::size_t len) {
    if (state_ == State::COMPLETE || state_ == State::ERROR) {
        return 0;
    }

    std::size_t consumed = 0;

    while (consumed < len && state_ != State::COMPLETE && state_ != State::ERROR) {
        if (state_ == State::READING_HEADER) {
            std::size_t remaining = kHeaderSize - buffer_offset_;
            std::size_t to_copy = std::min(remaining, len - consumed);
            std::memcpy(header_buf_ + buffer_offset_, data + consumed, to_copy);
            buffer_offset_ += to_copy;
            consumed += to_copy;

            if (buffer_offset_ == kHeaderSize) {
                auto parsed = deserializeHeader(header_buf_);
                if (!parsed) {
                    state_ = State::ERROR;
                    last_error_ = ErrorCode::INVALID_FRAME;
                    return consumed;
                }

                header_ = *parsed;

                // Validate header CRC
                if (!validateHeaderCrc(header_buf_, header_)) {
                    state_ = State::ERROR;
                    last_error_ = ErrorCode::CRC_MISMATCH;
                    return consumed;
                }

                // Check payload size limit
                if (header_.payload_len > max_payload_) {
                    state_ = State::ERROR;
                    last_error_ = ErrorCode::PAYLOAD_TOO_LARGE;
                    return consumed;
                }

                if (header_.payload_len == 0) {
                    state_ = State::COMPLETE;
                } else {
                    payload_.resize(header_.payload_len);
                    buffer_offset_ = 0;
                    state_ = State::READING_PAYLOAD;
                }
            }
        } else if (state_ == State::READING_PAYLOAD) {
            std::size_t remaining = header_.payload_len - buffer_offset_;
            std::size_t to_copy = std::min(remaining, len - consumed);
            std::memcpy(payload_.data() + buffer_offset_, data + consumed, to_copy);
            buffer_offset_ += to_copy;
            consumed += to_copy;

            if (buffer_offset_ == header_.payload_len) {
                state_ = State::COMPLETE;
            }
        }
    }

    return consumed;
}

std::vector<std::uint8_t> FrameParser::takePayload() {
    auto result = std::move(payload_);
    reset();
    return result;
}

void FrameParser::reset() {
    state_ = State::READING_HEADER;
    bytes_needed_ = kHeaderSize;
    buffer_offset_ = 0;
    header_ = FrameHeader{};
    payload_.clear();
    last_error_ = ErrorCode::OK;
}

} // namespace mfms::protocol
