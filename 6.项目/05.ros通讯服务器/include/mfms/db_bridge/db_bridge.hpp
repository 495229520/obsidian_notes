#pragma once

#include "mfms/common/types.hpp"
#include "mfms/common/config.hpp"
#include "mfms/common/queue.hpp"
#include "mfms/codec/codec.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <variant>

namespace mfms::reactor {
class Reactor;
}

namespace mfms::db_bridge {

// Event types for immediate DB writes
enum class EventType {
    ROBOT_CONNECTED,
    ROBOT_DISCONNECTED,
    ROBOT_FAULT,
    SERVICE_RESULT,
    PROTOCOL_VIOLATION,
    CLIENT_CONNECTED,
    CLIENT_DISCONNECTED,
};

// Event record for DB
struct EventRecord {
    EventType type;
    std::string entity_id;      // robot_id or client_id
    std::int64_t timestamp_ns;
    std::string details_json;
};

// DB job types
struct CheckpointStateJob {
    std::string robot_id;
    codec::NormalizedStatus status;
};

struct WriteEventJob {
    EventRecord event;
};

struct QueryJob {
    std::string query;
    std::function<void(bool success, std::string result)> callback;
};

using DbJobPayload = std::variant<CheckpointStateJob, WriteEventJob, QueryJob>;

struct DbJob {
    DbJobPayload payload;
    std::function<void(bool success, std::string message)> completion;
};

// MySQL connection wrapper (using mysql-connector-cpp or mysqlclient)
class DbConnection {
public:
    DbConnection(const Config& config);
    ~DbConnection();

    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }

    // Execute update/insert (returns affected rows or -1 on error)
    int execute(const std::string& sql);

    // Execute query (returns JSON result or empty on error)
    std::string query(const std::string& sql);

    // Prepared statements for common operations
    bool updateRobotState(const std::string& robot_id, const std::string& state_json);
    bool insertEvent(const EventRecord& event);

    std::string lastError() const { return last_error_; }

private:
    Config config_;
    void* conn_{nullptr};  // MYSQL* or sql::Connection*
    bool connected_{false};
    std::string last_error_;
};

// DB worker pool
class DbBridge {
public:
    explicit DbBridge(const Config& config);
    ~DbBridge();

    // Non-copyable
    DbBridge(const DbBridge&) = delete;
    DbBridge& operator=(const DbBridge&) = delete;

    // Start worker threads
    bool start(reactor::Reactor* reactor);

    // Stop workers
    void stop();

    // Enqueue checkpoint job (low priority, batched)
    void enqueueCheckpoint(const std::string& robot_id, codec::NormalizedStatus status);

    // Enqueue event job (high priority, immediate)
    void enqueueEvent(EventRecord event);

    // Enqueue query job
    void enqueueQuery(const std::string& query,
                      std::function<void(bool, std::string)> callback);

private:
    void workerFunc(int worker_id);

    Config config_;
    reactor::Reactor* reactor_{nullptr};

    std::vector<std::thread> workers_;
    MpmcQueue<DbJob> job_queue_;
    std::atomic<bool> running_{false};
};

} // namespace mfms::db_bridge
