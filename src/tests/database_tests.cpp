// File: src/tests/database_tests.cpp

#include <gtest/gtest.h>
#include "core/database/database.h"
#include <sqlite3.h>
#include <filesystem>

/**
 * @brief Test Fixture for Database tests.
 *
 * Sets up a temporary database for testing and cleans up afterward.
 */
class DatabaseTest : public ::testing::Test {
protected:
    std::string dbPath;  /**< Path to the test database file */
    Database* db;        /**< Pointer to the Database instance */

    /**
     * @brief Set up the test environment before each test.
     *
     * Creates a new Database instance and initializes it.
     */
    void SetUp() override {
        dbPath = "test_resource_monitor.db";
        // Remove if exists
        if (std::filesystem::exists(dbPath)) {
            std::filesystem::remove(dbPath);
        }
        db = new Database(dbPath);
        db->initialize();
    }

    /**
     * @brief Clean up after each test.
     *
     * Deletes the Database instance and removes the test database file.
     */
    void TearDown() override {
        delete db;
        if (std::filesystem::exists(dbPath)) {
            std::filesystem::remove(dbPath);
        }
    }
};

/**
 * @test Tests if the database initialization creates the necessary tables.
 */
TEST_F(DatabaseTest, InitializeCreatesTables) {
    // Check if tables exist by querying sqlite_master
    sqlite3* handle;
    ASSERT_EQ(sqlite3_open(dbPath.c_str(), &handle), SQLITE_OK);

    const char* tables[] = {"cpu_data", "memory_data", "network_data"};

    for (const char* table : tables) {
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + std::string(table) + "';";
        sqlite3_stmt* stmt;
        ASSERT_EQ(sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt, NULL), SQLITE_OK);
        ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
        const unsigned char* name = sqlite3_column_text(stmt, 0);
        EXPECT_STREQ(reinterpret_cast<const char*>(name), table);
        sqlite3_finalize(stmt);
    }

    sqlite3_close(handle);
}

/**
 * @test Tests inserting and retrieving CPU data from the database.
 */
TEST_F(DatabaseTest, InsertAndRetrieveCPUData) {
    // Insert sample data
    db->insertCPUData(45.5f, 3.6f, 10, 20, 85.0f, 50.0f);

    // Open database
    sqlite3* handle;
    ASSERT_EQ(sqlite3_open(dbPath.c_str(), &handle), SQLITE_OK);

    // Query the inserted data
    const char* sql = "SELECT total_usage, clock_frequency, used_threads, total_threads, highest_usage, average_usage FROM cpu_data;";
    sqlite3_stmt* stmt;
    ASSERT_EQ(sqlite3_prepare_v2(handle, sql, -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);

    float total_usage = static_cast<float>(sqlite3_column_double(stmt, 0));
    float clock_freq = static_cast<float>(sqlite3_column_double(stmt, 1));
    int used_threads = sqlite3_column_int(stmt, 2);
    int total_threads = sqlite3_column_int(stmt, 3);
    float highest_usage = static_cast<float>(sqlite3_column_double(stmt, 4));
    float average_usage = static_cast<float>(sqlite3_column_double(stmt, 5));

    EXPECT_FLOAT_EQ(total_usage, 45.5f);
    EXPECT_FLOAT_EQ(clock_freq, 3.6f);
    EXPECT_EQ(used_threads, 10);
    EXPECT_EQ(total_threads, 20);
    EXPECT_FLOAT_EQ(highest_usage, 85.0f);
    EXPECT_FLOAT_EQ(average_usage, 50.0f);

    sqlite3_finalize(stmt);
    sqlite3_close(handle);
}

// Additional tests can be implemented similarly for Memory and Network data


