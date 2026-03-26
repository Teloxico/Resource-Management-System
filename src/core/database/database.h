/**
 * @file database.h
 * @brief SQLite database for persisting resource monitoring metrics.
 *
 * Uses WAL journal mode for concurrent read performance and prepared
 * statements to prevent SQL injection.  Supports batch inserts via
 * explicit transactions.
 */

#pragma once

#include "../metrics.h"
#include <string>
#include <mutex>

struct sqlite3;
struct sqlite3_stmt;

class Database {
public:
    explicit Database(const std::string& db_path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /// Create tables and enable WAL.  Returns false on failure.
    bool initialize();

    /// Insert a full MetricData snapshot (CPU, Memory, Network, Disk, GPU).
    void insertSnapshot(const MetricData& data);

    /// Insert an alert event.
    void insertAlertEvent(const AlertEvent& ev);

    /// Delete data older than @p days days.
    void pruneOlderThan(int days);

    /// Export all tables to CSV files in @p directory.
    void exportToCSV(const std::string& directory = ".");

    /// Export selected tables filtered by timeframe.
    /// @p timeframeHours  Only rows from the last N hours (<=0 exports all).
    /// @p cpu,memory,network,disk,gpu  Select which tables to export.
    /// @p csvFormat  true = comma-separated .csv, false = tab-separated .txt.
    void exportFiltered(const std::string& directory,
                        int timeframeHours,
                        bool cpu, bool memory, bool network, bool disk, bool gpu,
                        bool csvFormat);

private:
    sqlite3*      db_     = nullptr;
    std::string   dbPath_;
    mutable std::mutex mtx_;

    // Prepared statements (lazily initialised in initialize())
    sqlite3_stmt* stmtCpu_     = nullptr;
    sqlite3_stmt* stmtMem_     = nullptr;
    sqlite3_stmt* stmtNet_     = nullptr;
    sqlite3_stmt* stmtDisk_    = nullptr;
    sqlite3_stmt* stmtGpu_     = nullptr;
    sqlite3_stmt* stmtAlert_   = nullptr;

    void prepareStatements();
    void finalizeStatements();
    bool exec(const char* sql);
    std::string currentTimestamp() const;
};
