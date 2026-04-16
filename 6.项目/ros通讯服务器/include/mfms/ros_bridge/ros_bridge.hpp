#pragma once

#include "mfms/common/types.hpp"
#include "mfms/common/config.hpp"
#include "mfms/reactor/reactor.hpp"
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace mfms::ros_bridge {

// Pending service call info
struct PendingServiceCall {
    RequestId request_id;
    ClientId client_id;
    std::string robot_id;
    TimePoint sent_at;
};

// ROS bridge node
class RosBridgeNode : public rclcpp::Node {
public:
    explicit RosBridgeNode(const Config& config);

    // Set callback for status updates (called from ROS thread)
    using StatusCallback = std::function<void(reactor::RosStatusUpdate)>;
    void setStatusCallback(StatusCallback cb) { status_callback_ = std::move(cb); }

    // Set callback for service completions (called from ROS thread)
    using CompletionCallback = std::function<void(reactor::RosServiceCompletion)>;
    void setCompletionCallback(CompletionCallback cb) { completion_callback_ = std::move(cb); }

    // Send command to robot (thread-safe, can be called from reactor)
    bool sendCommand(RequestId request_id, ClientId client_id,
                     const std::string& robot_id, const std::string& command_json,
                     uint32_t timeout_ms);

    // Send Lua script to robot (thread-safe)
    bool sendLuaScript(RequestId request_id, ClientId client_id,
                       const std::string& robot_id, const std::string& script_path);

private:
    void onRobotStatus(const std_msgs::msg::String::SharedPtr msg);

    Config config_;
    StatusCallback status_callback_;
    CompletionCallback completion_callback_;

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_sub_;

    // Service clients for each robot (created on demand)
    std::mutex clients_mutex_;
    std::unordered_map<std::string, rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr> service_clients_;

    // Pending service calls
    std::mutex pending_mutex_;
    std::unordered_map<std::string, PendingServiceCall> pending_calls_;
};

// ROS bridge thread wrapper
class RosBridge {
public:
    explicit RosBridge(const Config& config);
    ~RosBridge();

    // Non-copyable
    RosBridge(const RosBridge&) = delete;
    RosBridge& operator=(const RosBridge&) = delete;

    // Start the ROS thread
    bool start(reactor::Reactor* reactor);

    // Stop the ROS thread
    void stop();

    // Send command to robot (thread-safe)
    bool sendCommand(RequestId request_id, ClientId client_id,
                     const std::string& robot_id, const std::string& command_json,
                     uint32_t timeout_ms);

    // Send Lua script to robot (thread-safe)
    bool sendLuaScript(RequestId request_id, ClientId client_id,
                       const std::string& robot_id, const std::string& script_path);

private:
    void threadFunc();

    Config config_;
    reactor::Reactor* reactor_{nullptr};
    std::shared_ptr<RosBridgeNode> node_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace mfms::ros_bridge
