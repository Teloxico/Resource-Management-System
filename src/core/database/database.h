// File: src/core/database/database.h

#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <sqlite3.h>

/**
 * @class Database
 * @brief Handles database operations for resource monitoring data.
 *
 * Provides methods to initialize the database, insert data, and export data to CSV files.
 */
class Database {
public:
    /**
     * @brief Constructor for the Database class.
     * @param db_path Path to the SQLite database file.
     */
    Database(const std::string& db_path);

    /**
     * @brief Destructor for the Database class.
     *
     * Closes the database connection.
     */
    ~Database();

    /**
     * @brief Initializes the database by creating necessary tables.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize();

    /**
     * @brief Inserts CPU data into the database.
     * @param total_usage Total CPU usage percentage.
     * @param clock_frequency CPU clock frequency in GHz.
     * @param used_threads Number of threads used by the current process.
     * @param total_threads Total number of threads in the system.
     * @param highest_usage Highest CPU usage recorded.
     * @param average_usage Average CPU usage over time.
     */
    void insertCPUData(float total_usage, float clock_frequency, int used_threads, int total_threads, float highest_usage, float average_usage);

    /**
     * @brief Inserts Memory data into the database.
     * @param total_usage Total memory usage percentage.
     * @param remaining_ram Remaining RAM in MB.
     * @param average_usage Average memory usage over time.
     * @param top_process Name of the process using the most memory.
     */
    void insertMemoryData(float total_usage, float remaining_ram, float average_usage, const std::string& top_process);

    /**
     * @brief Inserts Network data into the database.
     * @param upload_rate Current upload rate in MB/s.
     * @param download_rate Current download rate in MB/s.
     * @param total_used_bandwidth Total used bandwidth in MB.
     */
    void insertNetworkData(float upload_rate, float download_rate, float total_used_bandwidth);

    /**
     * @brief Exports data from the database tables to CSV files.
     *
     * Creates CSV files in the root directory for CPU, Memory, and Network data.
     */
    void exportToCSV();

private:
    sqlite3* db_;           /**< Pointer to the SQLite database connection */
    std::string db_path_;   /**< Path to the database file */
};

#endif // DATABASE_H
