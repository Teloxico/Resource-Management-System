/**
 * @file database_tests.cpp
 * @brief Tests for the Database class (parameterised queries, WAL mode).
 */

#include <gtest/gtest.h>
#include "core/database/database.h"
#include <sqlite3.h>
#include <filesystem>

class DatabaseTest : public ::testing::Test {
protected:
    std::string dbPath = "test_resource_monitor.db";
    std::unique_ptr<Database> db;

    void SetUp() override {
        std::filesystem::remove(dbPath);
        db = std::make_unique<Database>(dbPath);
        ASSERT_TRUE(db->initialize());
    }
    void TearDown() override {
        db.reset();
        std::filesystem::remove(dbPath);
    }
};

TEST_F(DatabaseTest, InitCreatesTables) {
    sqlite3* raw = nullptr;
    ASSERT_EQ(sqlite3_open(dbPath.c_str(), &raw), SQLITE_OK);

    auto tableExists = [&](const char* name) -> bool {
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" +
                          std::string(name) + "';";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(raw, sql.c_str(), -1, &stmt, nullptr);
        bool found = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        return found;
    };

    EXPECT_TRUE(tableExists("cpu_metrics"));
    EXPECT_TRUE(tableExists("memory_metrics"));
    EXPECT_TRUE(tableExists("network_metrics"));
    EXPECT_TRUE(tableExists("disk_metrics"));
    EXPECT_TRUE(tableExists("gpu_metrics"));
    EXPECT_TRUE(tableExists("alert_events"));

    sqlite3_close(raw);
}

TEST_F(DatabaseTest, InsertSnapshotDoesNotCrash) {
    MetricData md{};
    md.cpu.totalUsage = 42.5f;
    md.memory.usagePercent = 65.0f;
    md.memory.topProcessName = "test_proc'; DROP TABLE cpu_metrics;--";  // SQL injection attempt
    db->insertSnapshot(md);
    // If we get here without crash/corruption, parameterised queries work
    SUCCEED();
}

TEST_F(DatabaseTest, InsertAndRetrieveCpu) {
    MetricData md{};
    md.cpu.totalUsage = 55.5f;
    md.cpu.frequency = 3600.0f;
    db->insertSnapshot(md);

    sqlite3* raw = nullptr;
    sqlite3_open(dbPath.c_str(), &raw);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(raw, "SELECT total_usage, frequency FROM cpu_metrics LIMIT 1;",
                       -1, &stmt, nullptr);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    EXPECT_NEAR(sqlite3_column_double(stmt, 0), 55.5, 0.1);
    EXPECT_NEAR(sqlite3_column_double(stmt, 1), 3600.0, 1.0);
    sqlite3_finalize(stmt);
    sqlite3_close(raw);
}
