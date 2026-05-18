#include "mfms/reactor/session.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <cstring>

namespace mfms::reactor {

Session::Session(ClientId id, int fd, const Config& config)
    : id_(id)
    , fd_(fd)
    , parser_(config.max_upload_size + 1024)  // Allow for header overhead
    , soft_limit_(config.send_buffer_soft_limit)
    , hard_limit_(config.send_buffer_hard_limit)
    , max_inflight_(config.max_inflight_requests)
    , last_activity_(Clock::now())
    , idle_timeout_sec_(config.idle_timeout_sec)
    , upload_timeout_sec_(config.upload_timeout_sec)
    , service_timeout_sec_(config.service_timeout_sec)
{
    send_buffer_.reserve(soft_limit_);
}

Session::~Session() {
    clearUpload();
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void Session::clearUpload() {
    if (upload_state_.has_value()) {
        if (upload_state_->temp_fd >= 0) {
            ::close(upload_state_->temp_fd);
            upload_state_->temp_fd = -1;
        }
        // Remove temp file if it exists
        if (!upload_state_->temp_path.empty()) {
            ::unlink(upload_state_->temp_path.c_str());
        }
        upload_state_.reset();
    }
}

bool Session::enqueueFrame(const protocol::FrameHeader& hdr, const std::uint8_t* payload) {
    if (send_buffer_.size() >= hard_limit_) {
        return false;
    }

    std::size_t frame_size = protocol::kHeaderSize + hdr.payload_len;
    send_buffer_.reserve(send_buffer_.size() + frame_size);

    // Serialize header
    std::uint8_t header_buf[protocol::kHeaderSize];
    protocol::serializeHeader(header_buf, hdr);
    send_buffer_.insert(send_buffer_.end(), header_buf, header_buf + protocol::kHeaderSize);

    // Append payload
    if (payload && hdr.payload_len > 0) {
        send_buffer_.insert(send_buffer_.end(), payload, payload + hdr.payload_len);
    }

    return true;
}

bool Session::enqueueFrame(const protocol::FrameHeader& hdr, const std::vector<std::uint8_t>& payload) {
    return enqueueFrame(hdr, payload.empty() ? nullptr : payload.data());
}

int Session::flushSendBuffer() {
    if (send_buffer_.empty()) {
        return 0;
    }

    std::size_t remaining = send_buffer_.size() - send_offset_;
    if (remaining == 0) {
        send_buffer_.clear();
        send_offset_ = 0;
        return 0;
    }

    ssize_t sent = ::send(fd_, send_buffer_.data() + send_offset_, remaining, MSG_NOSIGNAL);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }

    send_offset_ += static_cast<std::size_t>(sent);

    // Compact buffer if fully sent
    if (send_offset_ >= send_buffer_.size()) {
        send_buffer_.clear();
        send_offset_ = 0;
    } else if (send_offset_ > soft_limit_ / 2) {
        // Compact buffer to avoid unbounded growth
        send_buffer_.erase(send_buffer_.begin(), send_buffer_.begin() + send_offset_);
        send_offset_ = 0;
    }

    return static_cast<int>(sent);
}

bool Session::addInflight(const InflightRequest& req) {
    if (inflight_.size() >= max_inflight_) {
        return false;
    }
    inflight_[req.request_id] = req;
    return true;
}

std::optional<InflightRequest> Session::removeInflight(RequestId id) {
    auto it = inflight_.find(id);
    if (it == inflight_.end()) {
        return std::nullopt;
    }
    auto req = std::move(it->second);
    inflight_.erase(it);
    return req;
}

void Session::checkTimeouts(TimePoint now, std::function<void(const InflightRequest&)> on_timeout) {
    auto timeout = std::chrono::seconds(service_timeout_sec_);
    for (auto it = inflight_.begin(); it != inflight_.end();) {
        if (now - it->second.sent_at > timeout) {
            on_timeout(it->second);
            it = inflight_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace mfms::reactor
