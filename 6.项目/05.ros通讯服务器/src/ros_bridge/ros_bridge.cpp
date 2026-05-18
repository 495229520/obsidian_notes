#include "mfms/ros_bridge/ros_bridge.hpp"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>

namespace mfms::ros_bridge {

// ============ RosBridgeNode ============

RosBridgeNode::RosBridgeNode(const Config& config)
    : Node("mfms_server")
    , config_(config)
{
    // Subscribe to robot status topic
    // Using String for simplicity; real implementation would use custom message
    status_sub_ = this->create_subscription<std_msgs::msg::String>(
        config_.ros_status_topic,
        rclcpp::QoS(100).reliable(),
        std::bind(&RosBridgeNode::onRobotStatus, this, std::placeholders::_1)
    );

    spdlog::info("RosBridgeNode: subscribed to {}", config_.ros_status_topic);
}

void RosBridgeNode::onRobotStatus(const std_msgs::msg::String::SharedPtr msg) {
    // Parse the status message (JSON format expected)
    try {
        auto json = nlohmann::json::parse(msg->data);

        reactor::RosStatusUpdate update;
        update.robot_id = json.value("robot_id", "");
        update.robot_type = json.value("robot_type", "");
        update.timestamp_ns = json.value("timestamp_ns", 0LL);

        // Payload is the entire JSON as raw bytes
        std::string payload_str = json.value("payload", json.dump());
        update.raw_payload.assign(payload_str.begin(), payload_str.end());

        if (status_callback_) {
            status_callback_(std::move(update));
        }
    } catch (const std::exception& e) {
        spdlog::warn("RosBridgeNode: failed to parse status: {}", e.what());
    }
}

bool RosBridgeNode::sendCommand(RequestId request_id, ClientId client_id,
                                const std::string& robot_id, const std::string& command_json,
                                uint32_t timeout_ms) {
    // Get or create service client for this robot
    std::string service_name = config_.ros_service_prefix + robot_id + "/command";

    rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = service_clients_.find(robot_id);
        if (it == service_clients_.end()) {
            client = this->create_client<std_srvs::srv::Trigger>(service_name);
            service_clients_[robot_id] = client;
        } else {
            client = it->second;
        }
    }

    if (!client->wait_for_service(std::chrono::milliseconds(100))) {
        spdlog::warn("RosBridgeNode: service {} not available", service_name);
        if (completion_callback_) {
            reactor::RosServiceCompletion completion;
            completion.request_id = request_id;
            completion.client_id = client_id;
            completion.robot_id = robot_id;
            completion.success = false;
            completion.error_code = static_cast<int>(ErrorCode::ROBOT_NOT_FOUND);
            completion.result_json = R"({"error":"service not available"})";
            completion_callback_(completion);
        }
        return false;
    }

    // Store pending call
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_calls_[robot_id] = {request_id, client_id, robot_id, Clock::now()};
    }

    // Send async request
    auto request = std::make_shared<std_srvs::srv::Trigger::Request>();
    // Note: std_srvs::srv::Trigger doesn't have a data field
    // In real implementation, use custom service with command_json field

    auto future_callback = [this, robot_id, request_id, client_id](
        rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture future) {
        try {
            auto response = future.get();
            reactor::RosServiceCompletion completion;
            completion.request_id = request_id;
            completion.client_id = client_id;
            completion.robot_id = robot_id;
            completion.success = response->success;
            completion.error_code = response->success ? 0 : static_cast<int>(ErrorCode::SERVICE_FAILED);
            completion.result_json = response->message;

            if (completion_callback_) {
                completion_callback_(completion);
            }
        } catch (const std::exception& e) {
            spdlog::error("RosBridgeNode: service call failed for robot {}: {}", robot_id, e.what());
            if (completion_callback_) {
                reactor::RosServiceCompletion completion;
                completion.request_id = request_id;
                completion.client_id = client_id;
                completion.robot_id = robot_id;
                completion.success = false;
                completion.error_code = static_cast<int>(ErrorCode::SERVICE_FAILED);
                completion.result_json = std::string(R"({"error":")") + e.what() + "\"}";
                completion_callback_(completion);
            }
        }

        // Remove from pending
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_calls_.erase(robot_id);
    };

    client->async_send_request(request, future_callback);
    return true;
}

bool RosBridgeNode::sendLuaScript(RequestId request_id, ClientId client_id,
                                  const std::string& robot_id, const std::string& script_path) {
    // Similar to sendCommand but for Lua script forwarding
    // Implementation would use a different service type
    nlohmann::json cmd;
    cmd["action"] = "execute_lua";
    cmd["script_path"] = script_path;

    return sendCommand(request_id, client_id, robot_id, cmd.dump(), 30000);
}

// ============ RosBridge ============

RosBridge::RosBridge(const Config& config)
    : config_(config)
{}

RosBridge::~RosBridge() {
    stop();
}

bool RosBridge::start(reactor::Reactor* reactor) {
    if (running_) return false;

    reactor_ = reactor;
    running_ = true;
    thread_ = std::thread(&RosBridge::threadFunc, this);

    spdlog::info("RosBridge: started");
    return true;
}

void RosBridge::stop() {
    if (!running_) return;

    running_ = false;
    if (node_) {
        // Cancel all pending work
        rclcpp::shutdown();
    }
    if (thread_.joinable()) {
        thread_.join();
    }

    spdlog::info("RosBridge: stopped");
}

void RosBridge::threadFunc() {
    // Initialize ROS
    rclcpp::init(0, nullptr);

    node_ = std::make_shared<RosBridgeNode>(config_);

    // Set callbacks to post to reactor
    node_->setStatusCallback([this](reactor::RosStatusUpdate update) {
        if (reactor_) {
            reactor_->postRosStatus(std::move(update));
        }
    });

    node_->setCompletionCallback([this](reactor::RosServiceCompletion completion) {
        if (reactor_) {
            reactor_->postRosServiceCompletion(std::move(completion));
        }
    });

    // Spin until stopped
    rclcpp::executors::SingleThreadedExecutor executor;
    executor.add_node(node_);

    while (running_ && rclcpp::ok()) {
        executor.spin_some(std::chrono::milliseconds(100));
    }

    node_.reset();
    rclcpp::shutdown();
}

bool RosBridge::sendCommand(RequestId request_id, ClientId client_id,
                            const std::string& robot_id, const std::string& command_json,
                            uint32_t timeout_ms) {
    if (!node_) return false;
    return node_->sendCommand(request_id, client_id, robot_id, command_json, timeout_ms);
}

bool RosBridge::sendLuaScript(RequestId request_id, ClientId client_id,
                              const std::string& robot_id, const std::string& script_path) {
    if (!node_) return false;
    return node_->sendLuaScript(request_id, client_id, robot_id, script_path);
}

} // namespace mfms::ros_bridge
