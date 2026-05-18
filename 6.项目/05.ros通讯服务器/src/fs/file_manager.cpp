#include "mfms/fs/file_manager.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <vector>

namespace mfms::fs {

FileManager::FileManager(const Config& config)
    : config_(config)
    , data_root_(config.data_root)
{}

bool FileManager::init() {
    try {
        // Create base directories
        ensureDir(data_root_);
        ensureDir(data_root_ / config_.log_dir);
        ensureDir(data_root_ / config_.qt_log_dir);
        ensureDir(data_root_ / config_.lua_dir);
        ensureDir(data_root_ / config_.daily_dir);
        ensureDir(data_root_ / "tmp");

        spdlog::info("FileManager: initialized at {}", data_root_.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("FileManager: init failed: {}", e.what());
        return false;
    }
}

std::string FileManager::todayDateStr() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

std::string FileManager::getServerLogDir() const {
    return (data_root_ / config_.log_dir / todayDateStr()).string();
}

std::string FileManager::getQtLogDir(const std::string& client_id) const {
    return (data_root_ / config_.qt_log_dir / todayDateStr() / client_id).string();
}

std::string FileManager::getLuaDir(const std::string& robot_id) const {
    return (data_root_ / config_.lua_dir / todayDateStr() / robot_id).string();
}

std::string FileManager::getDailyDir() const {
    return (data_root_ / config_.daily_dir / todayDateStr()).string();
}

std::string FileManager::getTempDir() const {
    return (data_root_ / "tmp").string();
}

bool FileManager::ensureDir(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        return std::filesystem::is_directory(path);
    }
    return std::filesystem::create_directories(path);
}

int FileManager::createTempFile(const std::string& prefix, std::string& out_path) {
    auto temp_dir = data_root_ / "tmp";
    ensureDir(temp_dir);

    std::string template_path = (temp_dir / (prefix + "_XXXXXX")).string();
    std::vector<char> path_buf(template_path.begin(), template_path.end());
    path_buf.push_back('\0');

    int fd = mkstemp(path_buf.data());
    if (fd < 0) {
        spdlog::error("FileManager: mkstemp failed: {}", strerror(errno));
        return -1;
    }

    out_path = path_buf.data();
    return fd;
}

bool FileManager::finalizeUpload(int fd, const std::string& temp_path,
                                 const std::string& final_path) {
    // Ensure parent directory exists
    auto parent = std::filesystem::path(final_path).parent_path();
    if (!ensureDir(parent)) {
        spdlog::error("FileManager: cannot create dir {}", parent.string());
        close(fd);
        unlink(temp_path.c_str());
        return false;
    }

    // Sync to disk
    if (fsync(fd) < 0) {
        spdlog::error("FileManager: fsync failed: {}", strerror(errno));
        close(fd);
        unlink(temp_path.c_str());
        return false;
    }

    close(fd);

    // Atomic rename
    if (std::rename(temp_path.c_str(), final_path.c_str()) < 0) {
        spdlog::error("FileManager: rename {} -> {} failed: {}",
                      temp_path, final_path, strerror(errno));
        unlink(temp_path.c_str());
        return false;
    }

    spdlog::info("FileManager: finalized upload to {}", final_path);
    return true;
}

void FileManager::abortUpload(int fd, const std::string& temp_path) {
    if (fd >= 0) {
        close(fd);
    }
    if (!temp_path.empty()) {
        unlink(temp_path.c_str());
    }
}

std::string FileManager::readServerLog(const std::string& date, std::size_t offset,
                                       std::size_t max_size) {
    auto log_path = data_root_ / config_.log_dir / date / "server.log";

    if (!std::filesystem::exists(log_path)) {
        return "";
    }

    std::ifstream file(log_path, std::ios::binary);
    if (!file) {
        return "";
    }

    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file) {
        return "";
    }

    std::string content;
    if (max_size == 0) {
        // Read all
        std::ostringstream ss;
        ss << file.rdbuf();
        content = ss.str();
    } else {
        content.resize(max_size);
        file.read(&content[0], static_cast<std::streamsize>(max_size));
        content.resize(static_cast<std::size_t>(file.gcount()));
    }

    return content;
}

bool FileManager::generateDailyReport(const std::string& date,
                                      const std::function<std::string()>& report_generator) {
    auto report_dir = data_root_ / config_.daily_dir / date;
    if (!ensureDir(report_dir)) {
        return false;
    }

    auto report_path = report_dir / "report.json";
    std::string content = report_generator();

    std::ofstream file(report_path);
    if (!file) {
        spdlog::error("FileManager: cannot write report to {}", report_path.string());
        return false;
    }

    file << content;
    spdlog::info("FileManager: generated daily report at {}", report_path.string());
    return true;
}

void FileManager::cleanupTempFiles() {
    auto temp_dir = data_root_ / "tmp";
    if (!std::filesystem::exists(temp_dir)) {
        return;
    }

    auto now = std::filesystem::file_time_type::clock::now();
    auto max_age = std::chrono::hours(1);

    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
        try {
            auto mtime = std::filesystem::last_write_time(entry);
            if (now - mtime > max_age) {
                std::filesystem::remove(entry);
                spdlog::debug("FileManager: cleaned up old temp file {}", entry.path().string());
            }
        } catch (const std::exception& e) {
            spdlog::warn("FileManager: cleanup error: {}", e.what());
        }
    }
}

} // namespace mfms::fs
