/**
 * @file memory_linux.h
 * @brief Linux implementation of the Memory interface.
 */

#pragma once

#ifdef __linux__

#include "memory_common.h"

#include <vector>
#include <mutex>
#include <string>
#include <chrono>
#include <cstdint>

/**
 * @brief Collects memory metrics on Linux via /proc/meminfo, /proc/vmstat, and per-process /proc/[pid]/status.
 */
class LinuxMemory : public Memory {
public:
    LinuxMemory();
    ~LinuxMemory() override = default;

    /**
     * @brief Sample all memory metrics and update the internal snapshot.
     */
    void           update()                  override;

    /**
     * @brief Return a thread-safe copy of the latest snapshot.
     * @return Current MemorySnapshot.
     */
    MemorySnapshot snapshot() const          override;

private:
    std::chrono::steady_clock::time_point lastProcessScan_; ///< Last time process list was scanned.
    static constexpr int kProcessScanIntervalSec = 5;       ///< Seconds between process scans.
    std::string cachedTopName_;                              ///< Name of the top-memory process.
    uint64_t    cachedTopMem_ = 0;                           ///< RSS of the top process in bytes.

    uint64_t prevPgFault_ = 0;                               ///< Previous pgfault count from /proc/vmstat.
    std::chrono::steady_clock::time_point prevTime_;         ///< Timestamp of previous update call.

    static constexpr size_t kMaxHistory = 300;               ///< Max usage-history samples kept.
    std::vector<float> usageHistory_;                        ///< Rolling memory usage percentages.

    mutable std::mutex mutex_;                               ///< Guards current_ for thread safety.
    MemorySnapshot     current_;                             ///< Latest snapshot, protected by mutex_.

    /**
     * @brief Scan /proc to find the top 5 processes by RSS.
     * @param outName  Receives name of the highest-memory process.
     * @param outMem   Receives its RSS in bytes.
     * @param topProcs Receives up to 5 top processes sorted by memory.
     */
    void scanTopProcess(std::string& outName, uint64_t& outMem,
                        std::vector<MemorySnapshot::TopProcess>& topProcs);
    std::vector<MemorySnapshot::TopProcess> cachedTopProcs_; ///< Cached top-5 processes.
};

#endif
