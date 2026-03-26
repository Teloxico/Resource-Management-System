/**
 * @file process_linux.h
 * @brief Linux implementation of the ProcessManager interface.
 */

#pragma once

#ifdef __linux__

#include "process_common.h"

#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <string>
#include <cstdint>

/**
 * @class LinuxProcessManager
 * @brief Gathers process metrics on Linux via /proc filesystem.
 *
 * Iterates /proc/[pid]/ directories to read stat, status, cmdline,
 * and io files. Computes CPU% from utime/stime deltas.
 */
class LinuxProcessManager : public ProcessManager {
public:
    LinuxProcessManager();
    ~LinuxProcessManager() override;

    void             update()                               override;
    ProcessSnapshot  snapshot() const                       override;
    bool             killProcess(int pid)                   override;
    bool             setProcessPriority(int pid, int pri)   override;

private:
    // ---- per-process CPU delta tracking ----
    struct CpuTicks {
        unsigned long long utime = 0;
        unsigned long long stime = 0;
    };

    // ---- per-process I/O delta tracking ----
    struct IoBytes {
        int64_t readBytes  = 0;
        int64_t writeBytes = 0;
    };

    // ---- helpers ----
    bool parseStat(int pid, ProcessInfo& info, CpuTicks& ticks) const;
    bool parseStatus(int pid, ProcessInfo& info) const;
    std::string parseCmdline(int pid) const;
    bool parseIo(int pid, IoBytes& ioOut) const;
    std::string uidToName(unsigned int uid) const;

    // ---- state ----
    mutable std::mutex mtx_;
    ProcessSnapshot    snap_;

    /// Previous utime+stime per PID for CPU% delta computation.
    std::unordered_map<int, CpuTicks> prevTicks_;

    /// Previous I/O bytes per PID for rate computation.
    std::unordered_map<int, IoBytes> prevIo_;

    /// Wall-clock timestamp of the previous update() call.
    std::chrono::steady_clock::time_point prevWall_;
    bool hasPrevSample_ = false;

    /// Clock ticks per second (from sysconf).
    long clkTck_ = 100;

    /// Number of logical processors.
    int numProcessors_ = 1;

    /// Total physical memory in bytes (for memoryPercent).
    uint64_t totalMemBytes_ = 0;
};

#endif // __linux__
