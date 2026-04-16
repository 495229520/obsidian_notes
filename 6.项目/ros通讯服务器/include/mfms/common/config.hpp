#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace mfms {

struct Config {
    // Network
    std::uint16_t listen_port{9090};
    std::string listen_addr{"0.0.0.0"};
    int listen_backlog{128};

    // Session limits
    std::size_t max_clients{1000};
    std::size_t send_buffer_soft_limit{64 * 1024};   // 64 KB
    std::size_t send_buffer_hard_limit{256 * 1024};  // 256 KB
    std::size_t recv_buffer_size{64 * 1024};         // 64 KB
    std::size_t max_inflight_requests{16};

    // Timeouts (seconds)
    int idle_timeout_sec{300};          // 5 minutes
    int upload_timeout_sec{120};        // 2 minutes
    int service_timeout_sec{10};        // 10 seconds

    // Upload limits
    std::size_t max_upload_size{30 * 1024 * 1024};   // 30 MB
    std::size_t max_concurrent_uploads_per_client{1};

    // Status cache
    int snapshot_refresh_cooldown_ms{1000};  // 1 second

    // DB checkpointing
    int db_checkpoint_interval_sec{30};

    // File paths
    std::string data_root{"./data"};
    std::string log_dir{"logs/server"};
    std::string qt_log_dir{"logs/qt"};
    std::string lua_dir{"lua"};
    std::string daily_dir{"daily"};

    // MySQL
    std::string mysql_host{"127.0.0.1"};
    std::uint16_t mysql_port{3306};
    std::string mysql_user{"root"};
    std::string mysql_password{""};
    std::string mysql_database{"mfms_data"};
    int mysql_pool_size{4};

    // ROS
    std::string ros_status_topic{"/robot_status"};
    std::string ros_service_prefix{"/robot_"};
};

} // namespace mfms
