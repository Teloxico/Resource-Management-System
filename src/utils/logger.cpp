// File: src/utils/logger.cpp

#include "logger.h"
#include <fstream>
#include <chrono>
#include <iomanip>

std::mutex Logger::log_mutex_;
std::string Logger::log_file_path_ = "ResourceMonitor.log";

/**
 * @brief Initializes the logger with a specified log file path.
 * @param log_file_path Path to the log file.
 */
void Logger::initialize(const std::string& log_file_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_file_path_ = log_file_path;
}

/**
 * @brief Logs a message to the log file with a timestamp.
 * @param message The message to log.
 */
void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::ofstream logFile(log_file_path_, std::ios_base::app);
    if (logFile.is_open()) {
        // Get current time with high precision
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm local_tm;
    #ifdef _WIN32
        localtime_s(&local_tm, &now_time_t);
    #else
        localtime_r(&now_time_t, &local_tm);
    #endif

        logFile << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
                << '.' << std::setfill('0') << std::setw(3) << now_ms.count()
                << ": " << message << std::endl;
    }
}

