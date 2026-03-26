/**
 * @file logger.cpp
 * @brief Thread-safe logger implementation.
 */

#include "logger.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

std::mutex  Logger::log_mutex_;
std::string Logger::log_file_path_ = "ResourceMonitor.log";
LogLevel    Logger::min_level_     = LogLevel::Info;
bool        Logger::console_       = false;

void Logger::initialize(const std::string& path) {
    std::lock_guard<std::mutex> lk(log_mutex_);
    log_file_path_ = path;
}

void Logger::setLevel(LogLevel level)       { min_level_ = level; }
void Logger::setConsoleOutput(bool enabled) { console_   = enabled; }

void Logger::log(const std::string& msg) {
    log(LogLevel::Info, msg);
}

void Logger::debug(const std::string& msg) { log(LogLevel::Debug,   msg); }
void Logger::warn (const std::string& msg) { log(LogLevel::Warning, msg); }
void Logger::error(const std::string& msg) { log(LogLevel::Error,   msg); }

void Logger::log(LogLevel level, const std::string& message) {
    if (level < min_level_) return;

    static const char* tags[] = {"[DBG]","[INF]","[WRN]","[ERR]"};
    const char* tag = tags[static_cast<int>(level)];

    auto now   = std::chrono::system_clock::now();
    auto tt    = std::chrono::system_clock::to_time_t(now);
    auto ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now.time_since_epoch()) % 1000;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count()
       << ' ' << tag << ' ' << message;
    std::string line = ss.str();

    std::lock_guard<std::mutex> lk(log_mutex_);

    if (console_) {
        (level >= LogLevel::Warning ? std::cerr : std::cout) << line << '\n';
    }

    std::ofstream f(log_file_path_, std::ios_base::app);
    if (f.is_open()) f << line << '\n';
}
