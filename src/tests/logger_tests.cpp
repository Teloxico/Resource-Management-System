/**
 * @file logger_tests.cpp
 * @brief Tests for the Logger utility (severity levels, thread safety).
 */

#include <gtest/gtest.h>
#include "utils/logger.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <string>

class LoggerTest : public ::testing::Test {
protected:
    std::string logPath = "test_logger.log";
    void SetUp() override {
        std::filesystem::remove(logPath);
        Logger::initialize(logPath);
        Logger::setLevel(LogLevel::Debug);
    }
    void TearDown() override {
        std::filesystem::remove(logPath);
    }
};

TEST_F(LoggerTest, WritesToFile) {
    Logger::log("hello world");
    std::ifstream f(logPath);
    std::string line;
    ASSERT_TRUE(std::getline(f, line));
    EXPECT_NE(line.find("hello world"), std::string::npos);
    EXPECT_NE(line.find("[INF]"), std::string::npos);
}

TEST_F(LoggerTest, SeverityTags) {
    Logger::debug("dbg");
    Logger::warn("wrn");
    Logger::error("err");

    std::ifstream f(logPath);
    std::string lines, line;
    while (std::getline(f, line)) lines += line + "\n";

    EXPECT_NE(lines.find("[DBG]"), std::string::npos);
    EXPECT_NE(lines.find("[WRN]"), std::string::npos);
    EXPECT_NE(lines.find("[ERR]"), std::string::npos);
}

TEST_F(LoggerTest, LevelFiltering) {
    Logger::setLevel(LogLevel::Warning);
    Logger::debug("should not appear");
    Logger::log("should not appear either");
    Logger::warn("should appear");

    std::ifstream f(logPath);
    std::string lines, line;
    while (std::getline(f, line)) lines += line + "\n";

    EXPECT_EQ(lines.find("should not appear"), std::string::npos);
    EXPECT_NE(lines.find("should appear"), std::string::npos);
    Logger::setLevel(LogLevel::Debug); // reset
}

TEST_F(LoggerTest, ThreadSafety) {
    constexpr int N = 10;
    constexpr int M = 20;
    std::vector<std::thread> threads;
    for (int t = 0; t < N; ++t)
        threads.emplace_back([&, t]() {
            for (int i = 0; i < M; ++i)
                Logger::log("thread " + std::to_string(t) + " msg " + std::to_string(i));
        });
    for (auto& t : threads) t.join();

    std::ifstream f(logPath);
    int count = 0;
    std::string line;
    while (std::getline(f, line)) count++;
    EXPECT_EQ(count, N * M);
}
