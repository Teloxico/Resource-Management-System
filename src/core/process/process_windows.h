/**
 * @file process_windows.h
 * @brief Windows implementation of the ProcessManager interface.
 */

#pragma once

#ifdef _WIN32

#include "process_common.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <string>
#include <cstdint>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

/**
 * @class WindowsProcessManager
 * @brief Gathers process metrics on Windows via Toolhelp32, PSAPI, and
 *        standard Win32 process APIs.
 */
class WindowsProcessManager : public ProcessManager {
public:
    WindowsProcessManager();
    ~WindowsProcessManager() override;

    void             update()                               override;
    ProcessSnapshot  snapshot() const                       override;
    bool             killProcess(int pid)                   override;
    bool             setProcessPriority(int pid, int pri)   override;

private:
    // ---- per-process CPU delta tracking ----
    struct CpuTimes {
        ULONGLONG kernel = 0;
        ULONGLONG user   = 0;
    };

    // ---- per-process I/O delta tracking ----
    struct IoBytes {
        uint64_t readBytes  = 0;
        uint64_t writeBytes = 0;
    };

    // ---- helpers ----
    static ULONGLONG ftToU64(const FILETIME& ft);
    std::string      queryProcessPath(HANDLE hProc) const;
    std::string      queryProcessUser(HANDLE hProc) const;

    // ---- state ----
    mutable std::mutex mtx_;
    ProcessSnapshot    snap_;

    /// Previous kernel+user times per PID for CPU% delta computation.
    std::unordered_map<DWORD, CpuTimes> prevTimes_;

    /// Previous I/O bytes per PID for rate computation.
    std::unordered_map<DWORD, IoBytes> prevIo_;

    /// Wall-clock timestamp of the previous update() call.
    std::chrono::steady_clock::time_point prevWall_;
    bool hasPrevSample_ = false;

    /// Number of logical processors (for CPU% normalisation).
    int numProcessors_ = 1;
};

#endif // _WIN32
