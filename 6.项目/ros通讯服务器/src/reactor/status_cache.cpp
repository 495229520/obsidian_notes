#include "mfms/reactor/status_cache.hpp"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <type_traits>
#include <variant>

namespace mfms::reactor {

StatusCache::StatusCache(int refresh_cooldown_ms)
    : cooldown_(refresh_cooldown_ms)
    , last_rebuild_(Clock::now() - std::chrono::hours(1))  // Force initial rebuild
{
    // Initialize with empty snapshot
    snapshot_ = std::make_shared<std::string>("[]");
}

void StatusCache::updateStatus(const std::string& robot_id, codec::NormalizedStatus status) {
    latest_[robot_id] = std::move(status);
    last_update_[robot_id] = Clock::now();
    dirty_ = true;
}

void StatusCache::maybeRebuild() {
    if (!dirty_.load()) return;

    auto now = Clock::now();
    if (now - last_rebuild_ < cooldown_) return;

    rebuildSnapshot();
    dirty_ = false;
    last_rebuild_ = now;
}

void StatusCache::rebuildSnapshot() {
    nlohmann::json robots = nlohmann::json::array();

    for (const auto& robot_kv : latest_) {
        const auto& robot_id = robot_kv.first;
        const auto& status = robot_kv.second;
        nlohmann::json robot_json;

        for (const auto& kv : status) {
            const auto& key = kv.first;
            const auto& value = kv.second;
            std::visit([&robot_json, &key](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    robot_json[key] = nullptr;
                } else if constexpr (std::is_same_v<T, bool>) {
                    robot_json[key] = v;
                } else if constexpr (std::is_same_v<T, std::int64_t>) {
                    robot_json[key] = v;
                } else if constexpr (std::is_same_v<T, double>) {
                    robot_json[key] = v;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    robot_json[key] = v;
                } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
                    robot_json[key] = codec::base64Encode(v);
                }
            }, value);
        }

        robots.push_back(robot_json);
    }

    auto new_snapshot = std::make_shared<std::string>(robots.dump());

    // Thread-safe swap using mutex (C++17 compatible)
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        snapshot_ = new_snapshot;
    }

    spdlog::debug("Rebuilt status snapshot: {} robots, {} bytes",
                  latest_.size(), new_snapshot->size());
}

std::shared_ptr<const std::string> StatusCache::getSnapshot() const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return snapshot_;
}

const codec::NormalizedStatus* StatusCache::getRobotStatus(const std::string& robot_id) const {
    auto it = latest_.find(robot_id);
    return it != latest_.end() ? &it->second : nullptr;
}

TimePoint StatusCache::lastUpdateTime(const std::string& robot_id) const {
    auto it = last_update_.find(robot_id);
    return it != last_update_.end() ? it->second : TimePoint{};
}

} // namespace mfms::reactor
