/**
 * @file memory_windows.h
 * @brief Windows implementation of the Memory interface.
 */

#pragma once

#ifdef _WIN32

#include "memory_common.h"

#include <windows.h>
#include <psapi.h>
#include <pdh.h>

#include <vector>
#include <mutex>
#include <string>
#include <chrono>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "pdh.lib")

/**
 * @brief Collects memory metrics on Windows using GlobalMemoryStatusEx, GetPerformanceInfo, PDH counters, and EnumProcesses.
 */
class WindowsMemory : public Memory {
public:
    WindowsMemory();
    ~WindowsMemory() override;

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
    uint64_t    cachedTopMem_ = 0;                           ///< Working set of the top process in bytes.
    std::vector<MemorySnapshot::TopProcess> cachedTopProcs_; ///< Cached top-5 processes.

    std::chrono::steady_clock::time_point prevTime_;         ///< Timestamp of previous update call.

    PDH_HQUERY   pdhQuery_         = nullptr;  ///< PDH query handle.
    PDH_HCOUNTER pageFaultCounter_ = nullptr;  ///< PDH counter for page faults/sec.
    PDH_HCOUNTER cacheBytesCounter_ = nullptr; ///< PDH counter for cache bytes.
    bool         pdhReady_  = false;           ///< True when PDH counters are ready.
    bool         firstCollect_ = true;         ///< True until the first PDH collection completes.

    static constexpr size_t kMaxHistory = 300; ///< Max usage-history samples kept.
    std::vector<float> usageHistory_;          ///< Rolling memory usage percentages.

    mutable std::mutex mutex_;                 ///< Guards current_ for thread safety.
    MemorySnapshot     current_;               ///< Latest snapshot, protected by mutex_.

    /**
     * @brief Enumerate running processes and find the top 5 by working set.
     * @param outName  Name of the single highest-memory process.
     * @param outMem   Working set size in bytes of that process.
     * @param topProcs Populated with up to 5 top processes.
     */
    void scanTopProcess(std::string& outName, uint64_t& outMem,
                        std::vector<MemorySnapshot::TopProcess>& topProcs);
};

#endif
