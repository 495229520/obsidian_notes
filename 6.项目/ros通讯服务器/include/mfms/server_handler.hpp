#pragma once

#include "mfms/common/config.hpp"
#include "mfms/reactor/reactor.hpp"
#include "mfms/reactor/status_cache.hpp"
#include "mfms/codec/codec.hpp"
#include "mfms/ros_bridge/ros_bridge.hpp"
#include "mfms/db_bridge/db_bridge.hpp"
#include "mfms/fs/file_manager.hpp"
#include <memory>
#include <unordered_map>
#include <chrono>

namespace mfms {

// Main server handler implementing IReactorHandler
class ServerHandler : public reactor::IReactorHandler {
public:
    explicit ServerHandler(const Config& config);
    ~ServerHandler();

    bool init();
    void start();
    void stop();

    // IReactorHandler implementation
    void onFrame(reactor::Session& session, const protocol::FrameHeader& hdr,
                 std::vector<std::uint8_t> payload) override;
    void onDisconnect(reactor::Session& session, const std::string& reason) override;
    void onRosStatus(const reactor::RosStatusUpdate& update) override;
    void onRosServiceComplete(const reactor::RosServiceCompletion& completion) override;
    void onTimerTick() override;

private:
    // Frame handlers
    void handleStatusSnapshotReq(reactor::Session& session, const protocol::FrameHeader& hdr);
    void handleMotionReq(reactor::Session& session, const protocol::FrameHeader& hdr,
                         const std::vector<std::uint8_t>& payload);
    void handleUploadBegin(reactor::Session& session, const protocol::FrameHeader& hdr,
                           const std::vector<std::uint8_t>& payload, bool is_lua);
    void handleUploadChunk(reactor::Session& session, const protocol::FrameHeader& hdr,
                           const std::vector<std::uint8_t>& payload);
    void handleUploadEnd(reactor::Session& session, const protocol::FrameHeader& hdr,
                         const std::vector<std::uint8_t>& payload);
    void handleServerLogReq(reactor::Session& session, const protocol::FrameHeader& hdr,
                            const std::vector<std::uint8_t>& payload);

    // DB checkpointing
    void checkpointRobotStates();

    // Daily report generation
    void maybeGenerateDailyReport();

    Config config_;

    std::unique_ptr<reactor::Reactor> reactor_;
    std::unique_ptr<reactor::StatusCache> status_cache_;
    std::unique_ptr<codec::CodecRegistry> codec_registry_;
    std::unique_ptr<ros_bridge::RosBridge> ros_bridge_;
    std::unique_ptr<db_bridge::DbBridge> db_bridge_;
    std::unique_ptr<fs::FileManager> file_manager_;

    // Checkpoint tracking
    std::unordered_map<std::string, TimePoint> last_checkpoint_;
    std::chrono::seconds checkpoint_interval_;

    // Daily report tracking
    std::string last_report_date_;

    // Stats for daily report
    struct Stats {
        std::uint64_t frames_received{0};
        std::uint64_t frames_sent{0};
        std::uint64_t bytes_received{0};
        std::uint64_t bytes_sent{0};
        std::uint64_t uploads_completed{0};
        std::uint64_t uploads_failed{0};
        std::uint64_t motion_requests{0};
        std::uint64_t db_errors{0};
        std::uint64_t protocol_errors{0};
    };
    Stats stats_;
};

} // namespace mfms
