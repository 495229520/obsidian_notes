#pragma once

#include "mfms/common/types.hpp"
#include "mfms/common/config.hpp"
#include "mfms/protocol/frame.hpp"
#include "mfms/protocol/frame_parser.hpp"
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <optional>

namespace mfms::reactor {

// Upload session state
struct UploadState {
    UploadId upload_id{0};
    protocol::MsgType upload_type{};    // LUA_UPLOAD_BEGIN or QT_LOG_BEGIN
    std::string filename;
    std::string target_robot_id;
    std::string temp_path;              // Temp file path for cleanup
    std::uint32_t total_size{0};
    std::uint32_t received_size{0};
    std::uint32_t expected_crc32{0};
    int temp_fd{-1};
    TimePoint started_at;
    TimePoint last_chunk_at;
};

// In-flight request tracking
struct InflightRequest {
    RequestId request_id;
    protocol::MsgType msg_type;
    TimePoint sent_at;
    std::string robot_id;  // For motion/lua requests
};

// Per-client session
class Session {
public:
    Session(ClientId id, int fd, const Config& config);
    ~Session();

    // Non-copyable
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Accessors
    ClientId id() const { return id_; }
    int fd() const { return fd_; }
    const std::string& remoteAddr() const { return remote_addr_; }
    void setRemoteAddr(std::string addr) { remote_addr_ = std::move(addr); }

    // Read handling
    protocol::FrameParser& parser() { return parser_; }

    // Write buffer management
    bool hasOutboundData() const { return !send_buffer_.empty(); }
    std::size_t outboundSize() const { return send_buffer_.size(); }
    bool canAcceptMore() const { return send_buffer_.size() < soft_limit_; }
    bool isOverHardLimit() const { return send_buffer_.size() >= hard_limit_; }

    // Queue a frame for sending
    bool enqueueFrame(const protocol::FrameHeader& hdr, const std::uint8_t* payload = nullptr);
    bool enqueueFrame(const protocol::FrameHeader& hdr, const std::vector<std::uint8_t>& payload);

    // Send from buffer (returns bytes sent, -1 on error, 0 on EAGAIN)
    int flushSendBuffer();

    // Activity tracking
    void markActivity() { last_activity_ = Clock::now(); }
    TimePoint lastActivity() const { return last_activity_; }

    // Upload state
    bool hasActiveUpload() const { return upload_state_.has_value(); }
    UploadState& uploadState() { return *upload_state_; }
    const UploadState& uploadState() const { return *upload_state_; }
    void startUpload(UploadState state) { upload_state_ = std::move(state); }
    void clearUpload();

    // In-flight request tracking
    bool addInflight(const InflightRequest& req);
    std::optional<InflightRequest> removeInflight(RequestId id);
    std::size_t inflightCount() const { return inflight_.size(); }
    void checkTimeouts(TimePoint now, std::function<void(const InflightRequest&)> on_timeout);

private:
    ClientId id_;
    int fd_;
    std::string remote_addr_;

    protocol::FrameParser parser_;
    std::vector<std::uint8_t> send_buffer_;
    std::size_t send_offset_{0};
    std::size_t soft_limit_;
    std::size_t hard_limit_;
    std::size_t max_inflight_;

    TimePoint last_activity_;
    std::optional<UploadState> upload_state_;
    std::unordered_map<RequestId, InflightRequest> inflight_;

    int idle_timeout_sec_;
    int upload_timeout_sec_;
    int service_timeout_sec_;
};

} // namespace mfms::reactor
