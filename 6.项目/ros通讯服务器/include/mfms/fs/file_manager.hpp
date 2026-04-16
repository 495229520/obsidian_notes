#pragma once

#include "mfms/common/types.hpp"
#include "mfms/common/config.hpp"
#include <string>
#include <filesystem>
#include <functional>

namespace mfms::fs {

// File manager for organized storage
class FileManager {
public:
    explicit FileManager(const Config& config);

    // Initialize directories
    bool init();

    // Get paths
    std::string getServerLogDir() const;
    std::string getQtLogDir(const std::string& client_id) const;
    std::string getLuaDir(const std::string& robot_id) const;
    std::string getDailyDir() const;
    std::string getTempDir() const;

    // Get today's date string (YYYY-MM-DD)
    static std::string todayDateStr();

    // Create temp file for upload, returns fd and path
    int createTempFile(const std::string& prefix, std::string& out_path);

    // Finalize upload: fsync, close, atomic rename
    bool finalizeUpload(int fd, const std::string& temp_path,
                        const std::string& final_path);

    // Abort upload: close and remove temp file
    void abortUpload(int fd, const std::string& temp_path);

    // Read server log file (for serving to clients)
    std::string readServerLog(const std::string& date, std::size_t offset,
                              std::size_t max_size);

    // Generate daily report
    bool generateDailyReport(const std::string& date,
                             const std::function<std::string()>& report_generator);

    // Cleanup old temp files (on startup or periodic)
    void cleanupTempFiles();

private:
    bool ensureDir(const std::filesystem::path& path);

    Config config_;
    std::filesystem::path data_root_;
};

} // namespace mfms::fs
