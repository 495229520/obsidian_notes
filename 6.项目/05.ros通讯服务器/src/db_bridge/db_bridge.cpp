#include "mfms/db_bridge/db_bridge.hpp"
#include "mfms/reactor/reactor.hpp"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <mysql/mysql.h>
#include <chrono>
#include <variant>

namespace mfms::db_bridge {

// ============ DbConnection ============

DbConnection::DbConnection(const Config& config)
    : config_(config)
{}

DbConnection::~DbConnection() {
    disconnect();
}

bool DbConnection::connect() {
    if (connected_) return true;

    MYSQL* mysql = mysql_init(nullptr);
    if (!mysql) {
        last_error_ = "mysql_init failed";
        return false;
    }

    // Set connection timeout
    unsigned int timeout = 5;
    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    // Enable auto-reconnect
    bool reconnect = true;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(mysql,
                            config_.mysql_host.c_str(),
                            config_.mysql_user.c_str(),
                            config_.mysql_password.c_str(),
                            config_.mysql_database.c_str(),
                            config_.mysql_port,
                            nullptr, 0)) {
        last_error_ = mysql_error(mysql);
        mysql_close(mysql);
        return false;
    }

    // Set UTF-8
    mysql_set_character_set(mysql, "utf8mb4");

    conn_ = mysql;
    connected_ = true;
    spdlog::debug("DbConnection: connected to {}:{}", config_.mysql_host, config_.mysql_port);
    return true;
}

void DbConnection::disconnect() {
    if (conn_) {
        mysql_close(static_cast<MYSQL*>(conn_));
        conn_ = nullptr;
    }
    connected_ = false;
}

int DbConnection::execute(const std::string& sql) {
    if (!connected_) {
        if (!connect()) return -1;
    }

    MYSQL* mysql = static_cast<MYSQL*>(conn_);
    if (mysql_query(mysql, sql.c_str()) != 0) {
        last_error_ = mysql_error(mysql);
        spdlog::error("DbConnection: execute failed: {}", last_error_);
        return -1;
    }

    return static_cast<int>(mysql_affected_rows(mysql));
}

std::string DbConnection::query(const std::string& sql) {
    if (!connected_) {
        if (!connect()) return "";
    }

    MYSQL* mysql = static_cast<MYSQL*>(conn_);
    if (mysql_query(mysql, sql.c_str()) != 0) {
        last_error_ = mysql_error(mysql);
        spdlog::error("DbConnection: query failed: {}", last_error_);
        return "";
    }

    MYSQL_RES* result = mysql_store_result(mysql);
    if (!result) {
        last_error_ = mysql_error(mysql);
        return "";
    }

    nlohmann::json rows = nlohmann::json::array();
    unsigned int num_fields = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        nlohmann::json obj;
        for (unsigned int i = 0; i < num_fields; ++i) {
            if (row[i]) {
                obj[fields[i].name] = row[i];
            } else {
                obj[fields[i].name] = nullptr;
            }
        }
        rows.push_back(obj);
    }

    mysql_free_result(result);
    return rows.dump();
}

bool DbConnection::updateRobotState(const std::string& robot_id, const std::string& state_json) {
    // Escape strings
    MYSQL* mysql = static_cast<MYSQL*>(conn_);
    if (!mysql) return false;

    std::string escaped_id;
    escaped_id.resize(robot_id.size() * 2 + 1);
    unsigned long escaped_id_len =
        mysql_real_escape_string(mysql, &escaped_id[0], robot_id.c_str(), robot_id.size());
    escaped_id.resize(static_cast<std::size_t>(escaped_id_len));

    std::string escaped_json;
    escaped_json.resize(state_json.size() * 2 + 1);
    unsigned long escaped_json_len =
        mysql_real_escape_string(mysql, &escaped_json[0], state_json.c_str(), state_json.size());
    escaped_json.resize(static_cast<std::size_t>(escaped_json_len));

    std::string sql = "INSERT INTO device_state (id, state_enum, info, err_code) VALUES ('" +
                      escaped_id + "', 'online', '" + escaped_json + "', 0) " +
                      "ON DUPLICATE KEY UPDATE info = '" + escaped_json + "', state_enum = 'online'";

    return execute(sql) >= 0;
}

bool DbConnection::insertEvent(const EventRecord& event) {
    MYSQL* mysql = static_cast<MYSQL*>(conn_);
    if (!mysql) return false;

    std::string escaped_id;
    escaped_id.resize(event.entity_id.size() * 2 + 1);
    unsigned long escaped_id_len = mysql_real_escape_string(
        mysql, &escaped_id[0], event.entity_id.c_str(), event.entity_id.size());
    escaped_id.resize(static_cast<std::size_t>(escaped_id_len));

    std::string escaped_details;
    escaped_details.resize(event.details_json.size() * 2 + 1);
    unsigned long escaped_details_len = mysql_real_escape_string(
        mysql, &escaped_details[0], event.details_json.c_str(), event.details_json.size());
    escaped_details.resize(static_cast<std::size_t>(escaped_details_len));

    std::string event_type;
    switch (event.type) {
        case EventType::ROBOT_CONNECTED: event_type = "robot_connected"; break;
        case EventType::ROBOT_DISCONNECTED: event_type = "robot_disconnected"; break;
        case EventType::ROBOT_FAULT: event_type = "robot_fault"; break;
        case EventType::SERVICE_RESULT: event_type = "service_result"; break;
        case EventType::PROTOCOL_VIOLATION: event_type = "protocol_violation"; break;
        case EventType::CLIENT_CONNECTED: event_type = "client_connected"; break;
        case EventType::CLIENT_DISCONNECTED: event_type = "client_disconnected"; break;
    }

    std::string sql = "INSERT INTO robot_events (event_type, entity_id, timestamp_ns, details) VALUES ('" +
                      event_type + "', '" + escaped_id + "', " +
                      std::to_string(event.timestamp_ns) + ", '" + escaped_details + "')";

    return execute(sql) >= 0;
}

// ============ DbBridge ============

DbBridge::DbBridge(const Config& config)
    : config_(config)
    , job_queue_(10000)  // Max 10000 pending jobs
{}

DbBridge::~DbBridge() {
    stop();
}

bool DbBridge::start(reactor::Reactor* reactor) {
    if (running_) return false;

    reactor_ = reactor;
    running_ = true;

    // Create worker threads
    for (int i = 0; i < config_.mysql_pool_size; ++i) {
        workers_.emplace_back(&DbBridge::workerFunc, this, i);
    }

    spdlog::info("DbBridge: started {} workers", config_.mysql_pool_size);
    return true;
}

void DbBridge::stop() {
    if (!running_) return;

    running_ = false;
    job_queue_.stop();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();

    spdlog::info("DbBridge: stopped");
}

void DbBridge::enqueueCheckpoint(const std::string& robot_id, codec::NormalizedStatus status) {
    DbJob job;
    job.payload = CheckpointStateJob{robot_id, std::move(status)};
    // No completion callback for checkpoints (fire and forget)

    if (!job_queue_.push(std::move(job))) {
        spdlog::warn("DbBridge: job queue full, dropping checkpoint for {}", robot_id);
    }
}

void DbBridge::enqueueEvent(EventRecord event) {
    DbJob job;
    job.payload = WriteEventJob{std::move(event)};

    if (!job_queue_.push(std::move(job))) {
        spdlog::error("DbBridge: job queue full, dropping event");
    }
}

void DbBridge::enqueueQuery(const std::string& query,
                            std::function<void(bool, std::string)> callback) {
    DbJob job;
    QueryJob qj;
    qj.query = query;
    qj.callback = callback;
    job.payload = std::move(qj);
    job.completion = [callback](bool success, std::string msg) {
        // Completion handled via QueryJob callback
    };

    if (!job_queue_.push(std::move(job))) {
        spdlog::error("DbBridge: job queue full, dropping query");
        if (callback) callback(false, "queue full");
    }
}

void DbBridge::workerFunc(int worker_id) {
    spdlog::debug("DbBridge worker {} started", worker_id);

    DbConnection conn(config_);
    if (!conn.connect()) {
        spdlog::error("DbBridge worker {}: failed to connect to DB: {}", worker_id, conn.lastError());
    }

    while (running_) {
        DbJob job;
        try {
            job = job_queue_.pop();
        } catch (const std::runtime_error&) {
            // Queue stopped
            break;
        }

        bool success = false;
        std::string result;

        std::visit([&](auto&& payload) {
            using T = std::decay_t<decltype(payload)>;

            if constexpr (std::is_same_v<T, CheckpointStateJob>) {
                // Convert status to JSON and update DB
                std::string json = codec::normalizedStatusToJson(payload.status);
                success = conn.updateRobotState(payload.robot_id, json);
                if (!success) {
                    spdlog::warn("DbBridge worker {}: checkpoint failed for {}: {}",
                                 worker_id, payload.robot_id, conn.lastError());
                }
            }
            else if constexpr (std::is_same_v<T, WriteEventJob>) {
                success = conn.insertEvent(payload.event);
                if (!success) {
                    spdlog::error("DbBridge worker {}: event write failed: {}",
                                  worker_id, conn.lastError());
                }
            }
            else if constexpr (std::is_same_v<T, QueryJob>) {
                result = conn.query(payload.query);
                success = !result.empty() || conn.lastError().empty();

                if (payload.callback && reactor_) {
                    // Post callback to reactor
                    auto cb = payload.callback;
                    reactor_->postDbCompletion([cb, success, result]() {
                        cb(success, result);
                    });
                }
            }
        }, job.payload);

        // Call general completion if provided
        if (job.completion && reactor_) {
            auto cb = job.completion;
            reactor_->postDbCompletion([cb, success, result]() {
                cb(success, result);
            });
        }
    }

    conn.disconnect();
    spdlog::debug("DbBridge worker {} stopped", worker_id);
}

} // namespace mfms::db_bridge
