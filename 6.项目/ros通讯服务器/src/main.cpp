#include "mfms/server_handler.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  -p, --port PORT         Listen port (default: 9090)\n"
              << "  -d, --data-root PATH    Data directory (default: ./data)\n"
              << "  --mysql-host HOST       MySQL host (default: 127.0.0.1)\n"
              << "  --mysql-port PORT       MySQL port (default: 3306)\n"
              << "  --mysql-user USER       MySQL user (default: root)\n"
              << "  --mysql-password PASS   MySQL password\n"
              << "  --mysql-database DB     MySQL database (default: mfms_data)\n"
              << "  --ros-status-topic TOPIC ROS status topic (default: /robot_status)\n"
              << "  -v, --verbose           Enable debug logging\n"
              << "  -h, --help              Show this help\n";
}

mfms::Config parseArgs(int argc, char* argv[]) {
    mfms::Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "-v" || arg == "--verbose") {
            spdlog::set_level(spdlog::level::debug);
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.listen_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-d" || arg == "--data-root") && i + 1 < argc) {
            config.data_root = argv[++i];
        } else if (arg == "--mysql-host" && i + 1 < argc) {
            config.mysql_host = argv[++i];
        } else if (arg == "--mysql-port" && i + 1 < argc) {
            config.mysql_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--mysql-user" && i + 1 < argc) {
            config.mysql_user = argv[++i];
        } else if (arg == "--mysql-password" && i + 1 < argc) {
            config.mysql_password = argv[++i];
        } else if (arg == "--mysql-database" && i + 1 < argc) {
            config.mysql_database = argv[++i];
        } else if (arg == "--ros-status-topic" && i + 1 < argc) {
            config.ros_status_topic = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            std::exit(1);
        }
    }

    return config;
}

void setupLogging(const mfms::Config& config) {
    // Create rotating file sink
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        config.data_root + "/" + config.log_dir + "/server.log",
        10 * 1024 * 1024,  // 10 MB max
        5                   // 5 rotated files
    );

    // Create console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // Create logger with both sinks
    auto logger = std::make_shared<spdlog::logger>(
        "mfms",
        spdlog::sinks_init_list{file_sink, console_sink}
    );

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(1));
}

} // namespace

int main(int argc, char* argv[]) {
    // Parse arguments
    auto config = parseArgs(argc, argv);

    // Setup logging
    setupLogging(config);

    spdlog::info("MFMS Server starting...");
    spdlog::info("Config: port={}, data_root={}, mysql={}:{}/{}",
                 config.listen_port, config.data_root,
                 config.mysql_host, config.mysql_port, config.mysql_database);

    // Create and run server
    mfms::ServerHandler server(config);

    if (!server.init()) {
        spdlog::error("Server initialization failed");
        return 1;
    }

    spdlog::info("Server initialized, starting event loop...");
    server.start();

    spdlog::info("Server shutdown complete");
    return 0;
}
