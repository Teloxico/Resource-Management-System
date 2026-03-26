/**
 * @file database.cpp
 * @brief SQLite database implementation with parameterised statements.
 */

#include "database.h"
#include "../../utils/logger.h"
#include <sqlite3.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Database::Database(const std::string& db_path)
    : dbPath_(db_path)
{
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        Logger::log("DB: failed to open " + db_path + ": " +
                     std::string(sqlite3_errmsg(db_)));
        db_ = nullptr;
    }
}

Database::~Database() {
    finalizeStatements();
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

bool Database::initialize() {
    if (!db_) return false;

    // Enable WAL for better concurrent-read performance.
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");

    const char* tables[] = {
        "CREATE TABLE IF NOT EXISTS cpu_metrics ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  total_usage REAL, user_pct REAL, system_pct REAL,"
        "  frequency REAL, temperature REAL,"
        "  load_avg_1 REAL, load_avg_5 REAL, load_avg_15 REAL,"
        "  context_switches REAL, interrupts REAL,"
        "  core_count INTEGER, thread_count INTEGER);",

        "CREATE TABLE IF NOT EXISTS memory_metrics ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  usage_pct REAL, total_bytes INTEGER, used_bytes INTEGER,"
        "  available_bytes INTEGER, cached_bytes INTEGER, buffered_bytes INTEGER,"
        "  swap_total INTEGER, swap_used INTEGER, swap_pct REAL,"
        "  committed INTEGER, commit_limit INTEGER,"
        "  page_faults REAL, top_process TEXT);",

        "CREATE TABLE IF NOT EXISTS network_metrics ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  upload_rate REAL, download_rate REAL,"
        "  total_sent INTEGER, total_recv INTEGER,"
        "  interface_count INTEGER);",

        "CREATE TABLE IF NOT EXISTS disk_metrics ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  device TEXT, mount_point TEXT, fs_type TEXT,"
        "  usage_pct REAL, total_bytes INTEGER, used_bytes INTEGER,"
        "  read_rate REAL, write_rate REAL);",

        "CREATE TABLE IF NOT EXISTS gpu_metrics ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  name TEXT, utilization REAL,"
        "  memory_used INTEGER, memory_total INTEGER,"
        "  temperature REAL, power_watts REAL);",

        "CREATE TABLE IF NOT EXISTS alert_events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  rule_name TEXT, message TEXT,"
        "  value REAL, threshold REAL);",

        // Indexes on timestamp for fast range queries / pruning
        "CREATE INDEX IF NOT EXISTS idx_cpu_ts    ON cpu_metrics(timestamp);",
        "CREATE INDEX IF NOT EXISTS idx_mem_ts    ON memory_metrics(timestamp);",
        "CREATE INDEX IF NOT EXISTS idx_net_ts    ON network_metrics(timestamp);",
        "CREATE INDEX IF NOT EXISTS idx_disk_ts   ON disk_metrics(timestamp);",
        "CREATE INDEX IF NOT EXISTS idx_gpu_ts    ON gpu_metrics(timestamp);",
        "CREATE INDEX IF NOT EXISTS idx_alert_ts  ON alert_events(timestamp);",
    };

    for (auto& sql : tables) {
        if (!exec(sql)) return false;
    }

    prepareStatements();
    Logger::log("DB: initialised (" + dbPath_ + ")");
    return true;
}

// ---------------------------------------------------------------------------
// Prepared-statement helpers
// ---------------------------------------------------------------------------

void Database::prepareStatements() {
    if (!db_) return;

    auto prepare = [&](const char* sql, sqlite3_stmt*& stmt) {
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::log(std::string("DB: prepare failed: ") + sqlite3_errmsg(db_));
            stmt = nullptr;
        }
    };

    prepare("INSERT INTO cpu_metrics "
            "(timestamp,total_usage,user_pct,system_pct,frequency,temperature,"
            " load_avg_1,load_avg_5,load_avg_15,context_switches,interrupts,"
            " core_count,thread_count) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);", stmtCpu_);

    prepare("INSERT INTO memory_metrics "
            "(timestamp,usage_pct,total_bytes,used_bytes,available_bytes,"
            " cached_bytes,buffered_bytes,swap_total,swap_used,swap_pct,"
            " committed,commit_limit,page_faults,top_process) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);", stmtMem_);

    prepare("INSERT INTO network_metrics "
            "(timestamp,upload_rate,download_rate,total_sent,total_recv,"
            " interface_count) "
            "VALUES(?,?,?,?,?,?);", stmtNet_);

    prepare("INSERT INTO disk_metrics "
            "(timestamp,device,mount_point,fs_type,usage_pct,"
            " total_bytes,used_bytes,read_rate,write_rate) "
            "VALUES(?,?,?,?,?,?,?,?,?);", stmtDisk_);

    prepare("INSERT INTO gpu_metrics "
            "(timestamp,name,utilization,memory_used,memory_total,"
            " temperature,power_watts) "
            "VALUES(?,?,?,?,?,?,?);", stmtGpu_);

    prepare("INSERT INTO alert_events "
            "(timestamp,rule_name,message,value,threshold) "
            "VALUES(?,?,?,?,?);", stmtAlert_);
}

void Database::finalizeStatements() {
    auto fin = [](sqlite3_stmt*& s) { if (s) { sqlite3_finalize(s); s = nullptr; } };
    fin(stmtCpu_); fin(stmtMem_); fin(stmtNet_);
    fin(stmtDisk_); fin(stmtGpu_); fin(stmtAlert_);
}

// ---------------------------------------------------------------------------
// Insert a full snapshot in a single transaction
// ---------------------------------------------------------------------------

void Database::insertSnapshot(const MetricData& data) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return;

    std::string ts = currentTimestamp();

    exec("BEGIN TRANSACTION;");

    // ---- CPU ----
    if (stmtCpu_) {
        sqlite3_reset(stmtCpu_);
        sqlite3_bind_text (stmtCpu_, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmtCpu_, 2, data.cpu.totalUsage);
        sqlite3_bind_double(stmtCpu_, 3, data.cpu.userPercent);
        sqlite3_bind_double(stmtCpu_, 4, data.cpu.systemPercent);
        sqlite3_bind_double(stmtCpu_, 5, data.cpu.frequency);
        sqlite3_bind_double(stmtCpu_, 6, data.cpu.temperature);
        sqlite3_bind_double(stmtCpu_, 7, data.cpu.loadAvg1);
        sqlite3_bind_double(stmtCpu_, 8, data.cpu.loadAvg5);
        sqlite3_bind_double(stmtCpu_, 9, data.cpu.loadAvg15);
        sqlite3_bind_double(stmtCpu_,10, data.cpu.contextSwitchesPerSec);
        sqlite3_bind_double(stmtCpu_,11, data.cpu.interruptsPerSec);
        sqlite3_bind_int   (stmtCpu_,12, data.cpu.logicalCores);
        sqlite3_bind_int   (stmtCpu_,13, data.cpu.totalThreads);
        sqlite3_step(stmtCpu_);
    }

    // ---- Memory ----
    if (stmtMem_) {
        sqlite3_reset(stmtMem_);
        sqlite3_bind_text  (stmtMem_, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmtMem_, 2, data.memory.usagePercent);
        sqlite3_bind_int64 (stmtMem_, 3, static_cast<sqlite3_int64>(data.memory.totalBytes));
        sqlite3_bind_int64 (stmtMem_, 4, static_cast<sqlite3_int64>(data.memory.usedBytes));
        sqlite3_bind_int64 (stmtMem_, 5, static_cast<sqlite3_int64>(data.memory.availableBytes));
        sqlite3_bind_int64 (stmtMem_, 6, static_cast<sqlite3_int64>(data.memory.cachedBytes));
        sqlite3_bind_int64 (stmtMem_, 7, static_cast<sqlite3_int64>(data.memory.bufferedBytes));
        sqlite3_bind_int64 (stmtMem_, 8, static_cast<sqlite3_int64>(data.memory.swapTotal));
        sqlite3_bind_int64 (stmtMem_, 9, static_cast<sqlite3_int64>(data.memory.swapUsed));
        sqlite3_bind_double(stmtMem_,10, data.memory.swapPercent);
        sqlite3_bind_int64 (stmtMem_,11, static_cast<sqlite3_int64>(data.memory.committedBytes));
        sqlite3_bind_int64 (stmtMem_,12, static_cast<sqlite3_int64>(data.memory.commitLimitBytes));
        sqlite3_bind_double(stmtMem_,13, data.memory.pageFaultsPerSec);
        sqlite3_bind_text  (stmtMem_,14, data.memory.topProcessName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmtMem_);
    }

    // ---- Network ----
    if (stmtNet_) {
        sqlite3_reset(stmtNet_);
        sqlite3_bind_text  (stmtNet_, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmtNet_, 2, data.network.totalUploadRate);
        sqlite3_bind_double(stmtNet_, 3, data.network.totalDownloadRate);
        sqlite3_bind_int64 (stmtNet_, 4, static_cast<sqlite3_int64>(data.network.totalBytesSent));
        sqlite3_bind_int64 (stmtNet_, 5, static_cast<sqlite3_int64>(data.network.totalBytesRecv));
        sqlite3_bind_int   (stmtNet_, 6, static_cast<int>(data.network.interfaces.size()));
        sqlite3_step(stmtNet_);
    }

    // ---- Disk (one row per disk) ----
    if (stmtDisk_) {
        for (auto& d : data.disk.disks) {
            sqlite3_reset(stmtDisk_);
            sqlite3_bind_text  (stmtDisk_, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text  (stmtDisk_, 2, d.device.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text  (stmtDisk_, 3, d.mountPoint.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text  (stmtDisk_, 4, d.fsType.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmtDisk_, 5, d.usagePercent);
            sqlite3_bind_int64 (stmtDisk_, 6, static_cast<sqlite3_int64>(d.totalBytes));
            sqlite3_bind_int64 (stmtDisk_, 7, static_cast<sqlite3_int64>(d.usedBytes));
            sqlite3_bind_double(stmtDisk_, 8, d.readBytesPerSec);
            sqlite3_bind_double(stmtDisk_, 9, d.writeBytesPerSec);
            sqlite3_step(stmtDisk_);
        }
    }

    // ---- GPU (one row per GPU) ----
    if (stmtGpu_) {
        for (auto& g : data.gpu.gpus) {
            sqlite3_reset(stmtGpu_);
            sqlite3_bind_text  (stmtGpu_, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text  (stmtGpu_, 2, g.name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmtGpu_, 3, g.utilization);
            sqlite3_bind_int64 (stmtGpu_, 4, static_cast<sqlite3_int64>(g.memoryUsed));
            sqlite3_bind_int64 (stmtGpu_, 5, static_cast<sqlite3_int64>(g.memoryTotal));
            sqlite3_bind_double(stmtGpu_, 6, g.temperature);
            sqlite3_bind_double(stmtGpu_, 7, g.powerWatts);
            sqlite3_step(stmtGpu_);
        }
    }

    exec("COMMIT;");
}

// ---------------------------------------------------------------------------
// Alert events
// ---------------------------------------------------------------------------

void Database::insertAlertEvent(const AlertEvent& ev) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_ || !stmtAlert_) return;
    sqlite3_reset(stmtAlert_);
    sqlite3_bind_text  (stmtAlert_, 1, ev.timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmtAlert_, 2, ev.ruleName.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmtAlert_, 3, ev.message.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmtAlert_, 4, ev.value);
    sqlite3_bind_double(stmtAlert_, 5, ev.threshold);
    sqlite3_step(stmtAlert_);
}

// ---------------------------------------------------------------------------
// Pruning
// ---------------------------------------------------------------------------

void Database::pruneOlderThan(int days) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return;
    std::string cutoff = "datetime('now', '-" + std::to_string(days) + " days')";
    const char* tables[] = {
        "cpu_metrics","memory_metrics","network_metrics",
        "disk_metrics","gpu_metrics","alert_events"
    };
    for (auto& t : tables) {
        std::string sql = "DELETE FROM " + std::string(t) +
                          " WHERE timestamp < " + cutoff + ";";
        exec(sql.c_str());
    }
    Logger::log("DB: pruned data older than " + std::to_string(days) + " days");
}

// ---------------------------------------------------------------------------
// CSV export
// ---------------------------------------------------------------------------

void Database::exportToCSV(const std::string& directory) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return;

    struct TableDef {
        const char* table;
        const char* filename;
        const char* header;
    };

    TableDef defs[] = {
        {"cpu_metrics",    "cpu_metrics.csv",
         "timestamp,total_usage,user_pct,system_pct,frequency,temperature,"
         "load_avg_1,load_avg_5,load_avg_15,context_switches,interrupts,"
         "core_count,thread_count"},
        {"memory_metrics", "memory_metrics.csv",
         "timestamp,usage_pct,total_bytes,used_bytes,available_bytes,"
         "cached_bytes,buffered_bytes,swap_total,swap_used,swap_pct,"
         "committed,commit_limit,page_faults,top_process"},
        {"network_metrics","network_metrics.csv",
         "timestamp,upload_rate,download_rate,total_sent,total_recv,interface_count"},
        {"disk_metrics",   "disk_metrics.csv",
         "timestamp,device,mount_point,fs_type,usage_pct,total_bytes,"
         "used_bytes,read_rate,write_rate"},
        {"gpu_metrics",    "gpu_metrics.csv",
         "timestamp,name,utilization,memory_used,memory_total,temperature,power_watts"},
        {"alert_events",   "alert_events.csv",
         "timestamp,rule_name,message,value,threshold"},
    };

    for (auto& def : defs) {
        std::string sql = "SELECT * FROM " + std::string(def.table) + ";";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            continue;

        std::string path = directory + "/" + def.filename;
        std::ofstream f(path);
        if (!f.is_open()) {
            sqlite3_finalize(stmt);
            continue;
        }

        f << def.header << "\n";

        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            for (int i = 1; i < cols; ++i) {  // skip id column
                if (i > 1) f << ",";
                const char* val = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, i));
                if (val) f << val;
            }
            f << "\n";
        }

        sqlite3_finalize(stmt);
        Logger::log("DB: exported " + std::string(def.table) + " -> " + path);
    }
}

// ---------------------------------------------------------------------------
// Filtered export (CSV or TXT)
// ---------------------------------------------------------------------------

void Database::exportFiltered(const std::string& directory,
                              int timeframeHours,
                              bool cpu, bool memory, bool network,
                              bool disk, bool gpu,
                              bool csvFormat)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return;

    struct TableDef {
        const char* table;
        const char* baseName;
        const char* header;
        bool        enabled;
    };

    TableDef defs[] = {
        {"cpu_metrics",    "cpu_metrics",
         "timestamp,total_usage,user_pct,system_pct,frequency,temperature,"
         "load_avg_1,load_avg_5,load_avg_15,context_switches,interrupts,"
         "core_count,thread_count",
         cpu},
        {"memory_metrics", "memory_metrics",
         "timestamp,usage_pct,total_bytes,used_bytes,available_bytes,"
         "cached_bytes,buffered_bytes,swap_total,swap_used,swap_pct,"
         "committed,commit_limit,page_faults,top_process",
         memory},
        {"network_metrics","network_metrics",
         "timestamp,upload_rate,download_rate,total_sent,total_recv,interface_count",
         network},
        {"disk_metrics",   "disk_metrics",
         "timestamp,device,mount_point,fs_type,usage_pct,total_bytes,"
         "used_bytes,read_rate,write_rate",
         disk},
        {"gpu_metrics",    "gpu_metrics",
         "timestamp,name,utilization,memory_used,memory_total,temperature,power_watts",
         gpu},
    };

    const char* extension = csvFormat ? ".csv" : ".txt";
    const char* separator = csvFormat ? ","    : "\t";

    for (auto& def : defs) {
        if (!def.enabled) continue;

        // Build the query with an optional time filter.
        std::string sql;
        if (timeframeHours > 0) {
            sql = "SELECT * FROM " + std::string(def.table) +
                  " WHERE timestamp >= datetime('now', '-' || ? || ' hours');";
        } else {
            sql = "SELECT * FROM " + std::string(def.table) + ";";
        }

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            continue;

        // Bind the timeframe parameter when filtering is active.
        if (timeframeHours > 0) {
            sqlite3_bind_int(stmt, 1, timeframeHours);
        }

        std::string path = directory + "/" + def.baseName + extension;
        std::ofstream f(path);
        if (!f.is_open()) {
            sqlite3_finalize(stmt);
            continue;
        }

        // Write header — replace commas with the chosen separator.
        std::string hdr(def.header);
        if (!csvFormat) {
            for (auto& ch : hdr) {
                if (ch == ',') ch = '\t';
            }
        }
        f << hdr << "\n";

        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            for (int i = 1; i < cols; ++i) {  // skip id column
                if (i > 1) f << separator;
                const char* val = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, i));
                if (val) f << val;
            }
            f << "\n";
        }

        sqlite3_finalize(stmt);
        Logger::log("DB: exported " + std::string(def.table) + " -> " + path);
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

bool Database::exec(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        if (err) {
            Logger::log(std::string("DB exec error: ") + err);
            sqlite3_free(err);
        }
        return false;
    }
    return true;
}

std::string Database::currentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
