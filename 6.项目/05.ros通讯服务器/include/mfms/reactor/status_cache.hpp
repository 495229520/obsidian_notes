#pragma once

#include "mfms/common/types.hpp"
#include "mfms/codec/codec.hpp"
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include <string>
#include <mutex>

namespace mfms::reactor {

// Status cache for all robots
class StatusCache {
public:
    explicit StatusCache(int refresh_cooldown_ms = 1000);

    // Update status for a robot (called from reactor thread only)
    void updateStatus(const std::string& robot_id, codec::NormalizedStatus status);

    // Mark cache as dirty (rebuild needed)
    void markDirty() { dirty_ = true; }

    // Maybe rebuild snapshot if dirty and cooldown passed (called from reactor timer)
    void maybeRebuild();

    // Get current snapshot (thread-safe read via atomic load)
    std::shared_ptr<const std::string> getSnapshot() const;

    // Get single robot status (returns nullptr if not found)
    const codec::NormalizedStatus* getRobotStatus(const std::string& robot_id) const;

    // Get number of tracked robots
    std::size_t robotCount() const { return latest_.size(); }

    // Get last update time for a robot
    TimePoint lastUpdateTime(const std::string& robot_id) const;

private:
    void rebuildSnapshot();

    std::unordered_map<std::string, codec::NormalizedStatus> latest_;
    std::unordered_map<std::string, TimePoint> last_update_;

    // Snapshot is shared_ptr for thread-safe atomic swap (C++17 compatible)
    std::shared_ptr<std::string> snapshot_;
    mutable std::mutex snapshot_mutex_;

    TimePoint last_rebuild_;
    std::chrono::milliseconds cooldown_;
    std::atomic<bool> dirty_{false};
};

} // namespace mfms::reactor
