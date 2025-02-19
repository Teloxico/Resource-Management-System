// File: src/utils/logger.h

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>

/**
 * @class Logger
 * @brief Simple thread-safe logging utility.
 *
 * Provides static methods to initialize the logger and log messages
 * to a specified log file with timestamp information.
 */
class Logger {
public:
    /**
     * @brief Initializes the logger with a specified log file path.
     * @param log_file_path Path to the log file.
     */
    static void initialize(const std::string& log_file_path);

    /**
     * @brief Logs a message to the log file with a timestamp.
     * @param message The message to log.
     */
    static void log(const std::string& message);

private:
    static std::mutex log_mutex_;         /**< Mutex for thread safety */
    static std::string log_file_path_;    /**< Path to the log file */
};

#endif // LOGGER_H

