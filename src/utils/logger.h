/**
 * @file logger.h
 * @brief Thread-safe logger with severity levels and optional console output.
 */

#pragma once

#include <string>
#include <mutex>

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    static void initialize(const std::string& log_file_path);
    static void setLevel(LogLevel level);
    static void setConsoleOutput(bool enabled);

    static void log(const std::string& message);                     // Info
    static void log(LogLevel level, const std::string& message);
    static void debug(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

private:
    static std::mutex  log_mutex_;
    static std::string log_file_path_;
    static LogLevel    min_level_;
    static bool        console_;
};
