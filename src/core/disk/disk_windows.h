/**
 * @file disk_windows.h
 * @brief Windows disk monitoring implementation using Win32 and PDH.
 */

#pragma once

#ifdef _WIN32

#include "disk_common.h"

#include <windows.h>
#include <pdh.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "pdh.lib")

/**
 * @brief Windows disk monitor using GetDiskFreeSpaceEx and PDH I/O counters.
 */
class WindowsDisk : public Disk {
public:
    WindowsDisk();
    ~WindowsDisk() override;

    /**
     * @brief Refresh disk space and I/O rate data from the OS.
     */
    void         update()                override;

    /**
     * @brief Return a thread-safe copy of the current snapshot.
     * @return Latest DiskSnapshot.
     */
    DiskSnapshot snapshot() const        override;

private:
    /**
     * @brief Query free/total space for each logical drive.
     * @param snap Snapshot to populate with drive space info.
     */
    void queryDriveSpace(DiskSnapshot& snap);

    /**
     * @brief Read PDH I/O rate counters and assign to drives.
     * @param snap Snapshot to populate with I/O metrics.
     */
    void queryIOCounters(DiskSnapshot& snap);

    /**
     * @brief Discover per-physical-disk PDH counter instances.
     */
    void enumeratePdhInstances();

    PDH_HQUERY   pdhQuery_           = nullptr; ///< PDH query handle for all counters
    PDH_HCOUNTER readBytesCounter_   = nullptr; ///< Aggregate read bytes/sec counter
    PDH_HCOUNTER writeBytesCounter_  = nullptr; ///< Aggregate write bytes/sec counter
    PDH_HCOUNTER readOpsCounter_     = nullptr; ///< Aggregate read ops/sec counter
    PDH_HCOUNTER writeOpsCounter_    = nullptr; ///< Aggregate write ops/sec counter
    PDH_HCOUNTER diskTimeCounter_    = nullptr; ///< Aggregate disk time % counter
    bool         pdhReady_           = false;   ///< True once PDH is initialized
    bool         firstCollect_       = true;    ///< True until second PDH sample

    /**
     * @brief PDH counter set for one physical disk instance.
     */
    struct PdhDiskCounters {
        std::string instanceName;              ///< PDH instance name, e.g. "0 C:"
        PDH_HCOUNTER readBytes  = nullptr;     ///< Read bytes/sec for this disk
        PDH_HCOUNTER writeBytes = nullptr;     ///< Write bytes/sec for this disk
        PDH_HCOUNTER readOps    = nullptr;     ///< Read ops/sec for this disk
        PDH_HCOUNTER writeOps   = nullptr;     ///< Write ops/sec for this disk
        PDH_HCOUNTER diskTime   = nullptr;     ///< Disk time % for this disk
    };
    std::vector<PdhDiskCounters> perDiskCounters_;  ///< Per-physical-disk counters
    bool perDiskEnumerated_ = false;                ///< True after instance enumeration

    mutable std::mutex mutex_;   ///< Protects current_
    DiskSnapshot       current_; ///< Latest snapshot
};

#endif
