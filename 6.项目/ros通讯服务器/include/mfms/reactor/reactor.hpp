#pragma once

#include "mfms/common/types.hpp"
#include "mfms/common/config.hpp"
#include "mfms/common/queue.hpp"
#include "mfms/reactor/session.hpp"
#include "mfms/protocol/frame.hpp"
#include "mfms/protocol/messages.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <string>

namespace mfms::reactor {

// Forward declarations
class StatusCache;
class FileManager;

// ROS status update from ROS bridge
struct RosStatusUpdate {
    std::string robot_id;
    std::string robot_type;
    std::int64_t timestamp_ns;
    std::vector<std::uint8_t> raw_payload;
};

// Service call completion from ROS bridge
struct RosServiceCompletion {
    RequestId request_id;
    ClientId client_id;
    std::string robot_id;
    bool success;
    int error_code;
    std::string result_json;
};

// DB completion callback
struct DbCompletion {
    std::function<void()> callback;
};

// Reactor interface for handler callbacks
class IReactorHandler {
public:
    virtual ~IReactorHandler() = default;

    // Called when a complete frame is received
    virtual void onFrame(Session& session, const protocol::FrameHeader& hdr,
                         std::vector<std::uint8_t> payload) = 0;

    // Called when a client disconnects
    virtual void onDisconnect(Session& session, const std::string& reason) = 0;

    // Called when ROS status update arrives
    virtual void onRosStatus(const RosStatusUpdate& update) = 0;

    // Called when ROS service completes
    virtual void onRosServiceComplete(const RosServiceCompletion& completion) = 0;

    // Called on timer tick (1 second)
    virtual void onTimerTick() = 0;
};

class Reactor {
public:
    explicit Reactor(const Config& config);
    ~Reactor();

    // Non-copyable
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    // Initialize (create listening socket, epoll, timers, signals)
    bool init();

    // Set handler for events
    void setHandler(IReactorHandler* handler) { handler_ = handler; }

    // Run the event loop (blocks until stopped)
    void run();

    // Stop the reactor (thread-safe, can be called from signal handler or other thread)
    void stop();

    // Cross-thread communication: post ROS status (thread-safe)
    void postRosStatus(RosStatusUpdate update);

    // Cross-thread communication: post ROS service completion (thread-safe)
    void postRosServiceCompletion(RosServiceCompletion completion);

    // Cross-thread communication: post DB completion (thread-safe)
    void postDbCompletion(std::function<void()> callback);

    // Send response to client
    bool sendResponse(ClientId client_id, const protocol::FrameHeader& hdr,
                      const std::vector<std::uint8_t>& payload);
    bool sendError(ClientId client_id, RequestId request_id, ErrorCode code, const std::string& detail);
    bool sendAck(ClientId client_id, RequestId request_id);

    // Disconnect a client
    void disconnectClient(ClientId client_id, const std::string& reason);

    // Get session (or nullptr)
    Session* getSession(ClientId id);

    // Config access
    const Config& config() const { return config_; }

private:
    void acceptConnections();
    void handleRead(Session& session);
    void handleWrite(Session& session);
    void processFrame(Session& session);
    void drainRosQueue();
    void drainDbQueue();
    void checkTimeouts();
    void addToEpoll(int fd, uint32_t events, void* ptr);
    void modifyEpoll(int fd, uint32_t events, void* ptr);
    void removeFromEpoll(int fd);

    Config config_;
    IReactorHandler* handler_{nullptr};

    int listen_fd_{-1};
    int epoll_fd_{-1};
    int timer_fd_{-1};
    int signal_fd_{-1};
    int ros_eventfd_{-1};
    int db_eventfd_{-1};

    std::atomic<bool> running_{false};
    ClientId next_client_id_{1};

    std::unordered_map<ClientId, std::unique_ptr<Session>> sessions_;
    std::unordered_map<int, ClientId> fd_to_client_;  // For epoll lookup

    // Cross-thread queues
    MpmcQueue<RosStatusUpdate> ros_status_queue_;
    MpmcQueue<RosServiceCompletion> ros_completion_queue_;
    MpmcQueue<DbCompletion> db_completion_queue_;
    std::atomic<bool> ros_wake_pending_{false};
    std::atomic<bool> db_wake_pending_{false};
};

} // namespace mfms::reactor
