// File: src/tests/logger_tests.cpp

#include <gtest/gtest.h>
#include "utils/logger.h"
#include <fstream>
#include <string>
#include <filesystem>
#include <thread>

/**
 * @brief Helper function to read the contents of the log file.
 * @param logPath Path to the log file.
 * @return Contents of the log file as a string.
 */
std::string readLogFile(const std::string& logPath) {
    std::ifstream logFile(logPath);
    std::stringstream buffer;
    buffer << logFile.rdbuf();
    return buffer.str();
}

/**
 * @brief Test Fixture for Logger tests.
 *
 * Sets up a temporary log file for testing and cleans up afterward.
 */
class LoggerTest : public ::testing::Test {
protected:
    std::string logPath;  /**< Path to the test log file */

    /**
     * @brief Set up the test environment before each test.
     *
     * Initializes the Logger with the test log file path.
     */
    void SetUp() override {
        // Set log file path
        logPath = "test_log.log";
        // Remove existing log file
        if (std::filesystem::exists(logPath)) {
            std::filesystem::remove(logPath);
        }
        // Initialize logger
        Logger::initialize(logPath);
    }

    /**
     * @brief Clean up after each test.
     *
     * Removes the test log file.
     */
    void TearDown() override {
        // Remove log file after tests
        if (std::filesystem::exists(logPath)) {
            std::filesystem::remove(logPath);
        }
    }
};

/**
 * @test Tests that logging a message writes it to the log file.
 */
TEST_F(LoggerTest, LogWritesToFile) {
    std::string message = "Test log message.";
    Logger::log(message);

    // Read the log file
    std::string logContent = readLogFile(logPath);

    // Check if the message exists in the log
    EXPECT_NE(logContent.find(message), std::string::npos);
}

/**
 * @test Tests that multiple log entries are correctly written to the log file.
 */
TEST_F(LoggerTest, MultipleLogEntries) {
    std::vector<std::string> messages = {"First message", "Second message", "Third message"};
    for (const auto& msg : messages) {
        Logger::log(msg);
    }

    // Read the log file
    std::string logContent = readLogFile(logPath);

    for (const auto& msg : messages) {
        EXPECT_NE(logContent.find(msg), std::string::npos);
    }
}

/**
 * @test Tests the thread safety of the Logger by logging from multiple threads.
 */
TEST_F(LoggerTest, ThreadSafetyTest) {
    std::vector<std::thread> threads;
    const int thread_count = 10;
    const int messages_per_thread = 10;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([this, i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                Logger::log("Thread " + std::to_string(i) + " message " + std::to_string(j));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Read the log file
    std::string logContent = readLogFile(logPath);

    // Verify that all messages are logged
    for (int i = 0; i < thread_count; ++i) {
        for (int j = 0; j < messages_per_thread; ++j) {
            std::string message = "Thread " + std::to_string(i) + " message " + std::to_string(j);
            EXPECT_NE(logContent.find(message), std::string::npos);
        }
    }
}


