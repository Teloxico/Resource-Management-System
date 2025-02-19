// File: src/core/database/database.cpp

#include "database.h"
#include "utils/logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

/**
 * @brief Constructor for the Database class.
 * @param db_path Path to the SQLite database file.
 */
Database::Database(const std::string& db_path)
    : db_(nullptr), db_path_(db_path)
{
    // Open the database connection
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        Logger::log("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
        db_ = nullptr;
    }
}

/**
 * @brief Destructor for the Database class.
 *
 * Closes the database connection.
 */
Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

/**
 * @brief Initializes the database by creating necessary tables.
 * @return True if initialization is successful, false otherwise.
 */
bool Database::initialize() {
    if (!db_) {
        Logger::log("Database connection is not open.");
        return false;
    }

    const char* cpu_table_sql = "CREATE TABLE IF NOT EXISTS cpu_data ("
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "total_usage REAL, "
        "clock_frequency REAL, "
        "used_threads INTEGER, "
        "total_threads INTEGER, "
        "highest_usage REAL, "
        "average_usage REAL);";

    const char* memory_table_sql = "CREATE TABLE IF NOT EXISTS memory_data ("
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "total_usage REAL, "
        "remaining_ram REAL, "
        "average_usage REAL, "
        "top_process TEXT);";

    const char* network_table_sql = "CREATE TABLE IF NOT EXISTS network_data ("
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "upload_rate REAL, "
        "download_rate REAL, "
        "total_used_bandwidth REAL);";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, cpu_table_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::log("Failed to create cpu_data table: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(db_, memory_table_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::log("Failed to create memory_data table: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(db_, network_table_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::log("Failed to create network_data table: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }

    Logger::log("Database initialized successfully.");
    return true;
}

/**
 * @brief Inserts CPU data into the database.
 * @param total_usage Total CPU usage percentage.
 * @param clock_frequency CPU clock frequency in GHz.
 * @param used_threads Number of threads used by the current process.
 * @param total_threads Total number of threads in the system.
 * @param highest_usage Highest CPU usage recorded.
 * @param average_usage Average CPU usage over time.
 */
void Database::insertCPUData(float total_usage, float clock_frequency, int used_threads, int total_threads, float highest_usage, float average_usage) {
    if (!db_) {
        Logger::log("Database connection is not open.");
        return;
    }

    std::string sql = "INSERT INTO cpu_data (total_usage, clock_frequency, used_threads, total_threads, highest_usage, average_usage) VALUES ("
        + std::to_string(total_usage) + ", "
        + std::to_string(clock_frequency) + ", "
        + std::to_string(used_threads) + ", "
        + std::to_string(total_threads) + ", "
        + std::to_string(highest_usage) + ", "
        + std::to_string(average_usage) + ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::log("Failed to insert CPU data: " + std::string(errMsg));
        sqlite3_free(errMsg);
    }
}

/**
 * @brief Inserts Memory data into the database.
 * @param total_usage Total memory usage percentage.
 * @param remaining_ram Remaining RAM in MB.
 * @param average_usage Average memory usage over time.
 * @param top_process Name of the process using the most memory.
 */
void Database::insertMemoryData(float total_usage, float remaining_ram, float average_usage, const std::string& top_process) {
    if (!db_) {
        Logger::log("Database connection is not open.");
        return;
    }

    std::string sql = "INSERT INTO memory_data (total_usage, remaining_ram, average_usage, top_process) VALUES ("
        + std::to_string(total_usage) + ", "
        + std::to_string(remaining_ram) + ", "
        + std::to_string(average_usage) + ", '"
        + top_process + "');";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::log("Failed to insert Memory data: " + std::string(errMsg));
        sqlite3_free(errMsg);
    }
}

/**
 * @brief Inserts Network data into the database.
 * @param upload_rate Current upload rate in MB/s.
 * @param download_rate Current download rate in MB/s.
 * @param total_used_bandwidth Total used bandwidth in MB.
 */
void Database::insertNetworkData(float upload_rate, float download_rate, float total_used_bandwidth) {
    if (!db_) {
        Logger::log("Database connection is not open.");
        return;
    }

    std::string sql = "INSERT INTO network_data (upload_rate, download_rate, total_used_bandwidth) VALUES ("
        + std::to_string(upload_rate) + ", "
        + std::to_string(download_rate) + ", "
        + std::to_string(total_used_bandwidth) + ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::log("Failed to insert Network data: " + std::string(errMsg));
        sqlite3_free(errMsg);
    }
}

/**
 * @brief Exports data from the database tables to CSV files.
 *
 * Creates CSV files in the root directory for CPU, Memory, and Network data.
 */
void Database::exportToCSV() {
    if (!db_) {
        Logger::log("Database connection is not open.");
        return;
    }

    // Export CPU data
    {
        std::string sql = "SELECT * FROM cpu_data;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::log("Failed to prepare statement for CPU data export.");
            return;
        }

        std::ofstream csvFile("cpu_data.csv");
        if (!csvFile.is_open()) {
            Logger::log("Failed to open cpu_data.csv for writing.");
            sqlite3_finalize(stmt);
            return;
        }

        // Write CSV header
        csvFile << "Timestamp,Total Usage (%),Clock Frequency (GHz),Used Threads,Total Threads,Highest Usage (%),Average Usage (%)\n";

        // Fetch and write data
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            float total_usage = static_cast<float>(sqlite3_column_double(stmt, 1));
            float clock_frequency = static_cast<float>(sqlite3_column_double(stmt, 2));
            int used_threads = sqlite3_column_int(stmt, 3);
            int total_threads = sqlite3_column_int(stmt, 4);
            float highest_usage = static_cast<float>(sqlite3_column_double(stmt, 5));
            float average_usage = static_cast<float>(sqlite3_column_double(stmt, 6));

            csvFile << timestamp << ","
                    << total_usage << ","
                    << clock_frequency << ","
                    << used_threads << ","
                    << total_threads << ","
                    << highest_usage << ","
                    << average_usage << "\n";
        }

        csvFile.close();
        sqlite3_finalize(stmt);
        Logger::log("CPU data exported to cpu_data.csv.");
    }

    // Export Memory data
    {
        std::string sql = "SELECT * FROM memory_data;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::log("Failed to prepare statement for Memory data export.");
            return;
        }

        std::ofstream csvFile("memory_data.csv");
        if (!csvFile.is_open()) {
            Logger::log("Failed to open memory_data.csv for writing.");
            sqlite3_finalize(stmt);
            return;
        }

        // Write CSV header
        csvFile << "Timestamp,Total Usage (%),Remaining RAM (MB),Average Usage (%),Top Process\n";

        // Fetch and write data
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            float total_usage = static_cast<float>(sqlite3_column_double(stmt, 1));
            float remaining_ram = static_cast<float>(sqlite3_column_double(stmt, 2));
            float average_usage = static_cast<float>(sqlite3_column_double(stmt, 3));
            std::string top_process = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

            csvFile << timestamp << ","
                    << total_usage << ","
                    << remaining_ram << ","
                    << average_usage << ","
                    << top_process << "\n";
        }

        csvFile.close();
        sqlite3_finalize(stmt);
        Logger::log("Memory data exported to memory_data.csv.");
    }

    // Export Network data
    {
        std::string sql = "SELECT * FROM network_data;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::log("Failed to prepare statement for Network data export.");
            return;
        }

        std::ofstream csvFile("network_data.csv");
        if (!csvFile.is_open()) {
            Logger::log("Failed to open network_data.csv for writing.");
            sqlite3_finalize(stmt);
            return;
        }

        // Write CSV header
        csvFile << "Timestamp,Upload Rate (MB/s),Download Rate (MB/s),Total Used Bandwidth (MB)\n";

        // Fetch and write data
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            float upload_rate = static_cast<float>(sqlite3_column_double(stmt, 1));
            float download_rate = static_cast<float>(sqlite3_column_double(stmt, 2));
            float total_used_bandwidth = static_cast<float>(sqlite3_column_double(stmt, 3));

            csvFile << timestamp << ","
                    << upload_rate << ","
                    << download_rate << ","
                    << total_used_bandwidth << "\n";
        }

        csvFile.close();
        sqlite3_finalize(stmt);
        Logger::log("Network data exported to network_data.csv.");
    }
}