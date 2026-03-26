/**
 * @file process_windows.cpp
 * @brief Windows process monitoring and management implementation.
 *
 * Uses CreateToolhelp32Snapshot for process enumeration, GetProcessTimes
 * for CPU deltas, GetProcessMemoryInfo for RSS, QueryFullProcessImageNameA
 * for executable path, and OpenProcessToken / LookupAccountSidA for user.
 */

#ifdef _WIN32

#include "process_windows.h"

#include <algorithm>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

ULONGLONG WindowsProcessManager::ftToU64(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

std::string WindowsProcessManager::queryProcessPath(HANDLE hProc) const {
    char buf[MAX_PATH]{};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameA(hProc, 0, buf, &size)) {
        return std::string(buf, size);
    }
    return {};
}

std::string WindowsProcessManager::queryProcessUser(HANDLE hProc) const {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(hProc, TOKEN_QUERY, &hToken))
        return {};

    // Determine required buffer size.
    DWORD tokenLen = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &tokenLen);
    if (tokenLen == 0) {
        CloseHandle(hToken);
        return {};
    }

    std::vector<BYTE> tokenBuf(tokenLen);
    if (!GetTokenInformation(hToken, TokenUser, tokenBuf.data(), tokenLen, &tokenLen)) {
        CloseHandle(hToken);
        return {};
    }
    CloseHandle(hToken);

    TOKEN_USER* pUser = reinterpret_cast<TOKEN_USER*>(tokenBuf.data());

    char name[256]{};
    char domain[256]{};
    DWORD nameLen   = sizeof(name);
    DWORD domainLen = sizeof(domain);
    SID_NAME_USE sidType;

    if (LookupAccountSidA(nullptr, pUser->User.Sid, name, &nameLen,
                           domain, &domainLen, &sidType)) {
        std::string result;
        if (domainLen > 0) {
            result += std::string(domain, domainLen);
            result += '\\';
        }
        result += std::string(name, nameLen);
        return result;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

WindowsProcessManager::WindowsProcessManager() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    numProcessors_ = static_cast<int>(si.dwNumberOfProcessors);
    if (numProcessors_ < 1) numProcessors_ = 1;
}

WindowsProcessManager::~WindowsProcessManager() = default;

// ---------------------------------------------------------------------------
// update()
// ---------------------------------------------------------------------------

void WindowsProcessManager::update() {
    ProcessSnapshot newSnap;

    auto now = std::chrono::steady_clock::now();
    double wallDeltaSec = 0.0;
    if (hasPrevSample_) {
        wallDeltaSec = std::chrono::duration<double>(now - prevWall_).count();
    }

    // Maps to hold new CPU times and I/O for delta tracking.
    std::unordered_map<DWORD, CpuTimes> newTimes;
    std::unordered_map<DWORD, IoBytes>  newIo;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        // Cannot enumerate — keep stale snapshot.
        return;
    }

    // Gather total physical memory once for memoryPercent calculation.
    MEMORYSTATUSEX memStat{};
    memStat.dwLength = sizeof(memStat);
    uint64_t totalPhysMem = 0;
    if (GlobalMemoryStatusEx(&memStat)) {
        totalPhysMem = memStat.ullTotalPhys;
    }

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);

    int totalThreads     = 0;
    int runningProcesses = 0;

    if (Process32First(hSnap, &pe)) {
        do {
            ProcessInfo info;
            info.pid     = static_cast<int>(pe.th32ProcessID);
            info.ppid    = static_cast<int>(pe.th32ParentProcessID);
            info.name    = pe.szExeFile;
            info.threads = static_cast<int>(pe.cntThreads);

            totalThreads += info.threads;

            // Attempt to open the process for detailed queries.
            HANDLE hProc = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                FALSE, pe.th32ProcessID);

            if (hProc) {
                // --- Memory ---
                PROCESS_MEMORY_COUNTERS pmc{};
                if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                    info.memoryBytes = static_cast<uint64_t>(pmc.WorkingSetSize);
                    if (totalPhysMem > 0) {
                        info.memoryPercent = static_cast<float>(info.memoryBytes)
                                             / static_cast<float>(totalPhysMem) * 100.0f;
                    }
                }

                // --- Path ---
                info.path = queryProcessPath(hProc);

                // --- User ---
                info.user = queryProcessUser(hProc);

                // --- CPU times ---
                FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
                if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                    CpuTimes cur;
                    cur.kernel = ftToU64(ftKernel);
                    cur.user   = ftToU64(ftUser);
                    newTimes[pe.th32ProcessID] = cur;

                    if (hasPrevSample_ && wallDeltaSec > 0.0) {
                        auto it = prevTimes_.find(pe.th32ProcessID);
                        if (it != prevTimes_.end()) {
                            ULONGLONG dKernel = cur.kernel - it->second.kernel;
                            ULONGLONG dUser   = cur.user   - it->second.user;
                            // FILETIME units are 100-nanosecond intervals.
                            double cpuTimeSec = static_cast<double>(dKernel + dUser) / 1.0e7;
                            info.cpuPercent   = static_cast<float>(
                                cpuTimeSec / (wallDeltaSec * numProcessors_) * 100.0);
                            if (info.cpuPercent < 0.0f)   info.cpuPercent = 0.0f;
                            if (info.cpuPercent > 100.0f) info.cpuPercent = 100.0f;
                        }
                    }
                }

                // --- State approximation ---
                // Windows does not expose a direct per-process state like Linux.
                // If CPU usage is non-zero the process is considered Running.
                if (info.cpuPercent > 0.01f) {
                    info.state = 'R';
                    ++runningProcesses;
                } else {
                    info.state = 'S'; // sleeping / waiting
                }

                // --- Priority ---
                DWORD priClass = GetPriorityClass(hProc);
                switch (priClass) {
                    case IDLE_PRIORITY_CLASS:         info.priority = -2; break;
                    case BELOW_NORMAL_PRIORITY_CLASS: info.priority = -1; break;
                    case NORMAL_PRIORITY_CLASS:       info.priority =  0; break;
                    case ABOVE_NORMAL_PRIORITY_CLASS: info.priority =  1; break;
                    case HIGH_PRIORITY_CLASS:         info.priority =  2; break;
                    case REALTIME_PRIORITY_CLASS:     info.priority =  3; break;
                    default:                          info.priority =  0; break;
                }

                // --- I/O (delta-based rates) ---
                {
                    IO_COUNTERS ioc{};
                    if (GetProcessIoCounters(hProc, &ioc)) {
                        IoBytes curIo;
                        curIo.readBytes  = ioc.ReadTransferCount;
                        curIo.writeBytes = ioc.WriteTransferCount;
                        newIo[pe.th32ProcessID] = curIo;

                        if (hasPrevSample_ && wallDeltaSec > 0.0) {
                            auto ioIt = prevIo_.find(pe.th32ProcessID);
                            if (ioIt != prevIo_.end()) {
                                uint64_t dRead  = curIo.readBytes  - ioIt->second.readBytes;
                                uint64_t dWrite = curIo.writeBytes - ioIt->second.writeBytes;
                                info.readBytesPerSec  = static_cast<int64_t>(
                                    static_cast<double>(dRead) / wallDeltaSec);
                                info.writeBytesPerSec = static_cast<int64_t>(
                                    static_cast<double>(dWrite) / wallDeltaSec);
                            }
                        }
                    }
                }

                CloseHandle(hProc);
            } else {
                // Could not open — still record basic info from toolhelp.
                info.state = '?';
            }

            newSnap.processes.push_back(std::move(info));
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);

    newSnap.totalProcesses   = static_cast<int>(newSnap.processes.size());
    newSnap.totalThreads     = totalThreads;
    newSnap.runningProcesses = runningProcesses;

    // --- Swap into shared state ---
    {
        std::lock_guard<std::mutex> lock(mtx_);
        snap_      = std::move(newSnap);
        prevTimes_ = std::move(newTimes);
        prevIo_    = std::move(newIo);
        prevWall_  = now;
        hasPrevSample_ = true;
    }
}

// ---------------------------------------------------------------------------
// snapshot()
// ---------------------------------------------------------------------------

ProcessSnapshot WindowsProcessManager::snapshot() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return snap_;
}

// ---------------------------------------------------------------------------
// killProcess()
// ---------------------------------------------------------------------------

bool WindowsProcessManager::killProcess(int pid) {
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!hProc) return false;
    BOOL ok = TerminateProcess(hProc, 1);
    CloseHandle(hProc);
    return ok != 0;
}

// ---------------------------------------------------------------------------
// setProcessPriority()
// ---------------------------------------------------------------------------

bool WindowsProcessManager::setProcessPriority(int pid, int priority) {
    DWORD priClass;
    switch (priority) {
        case -2: priClass = IDLE_PRIORITY_CLASS;         break;
        case -1: priClass = BELOW_NORMAL_PRIORITY_CLASS; break;
        case  0: priClass = NORMAL_PRIORITY_CLASS;       break;
        case  1: priClass = ABOVE_NORMAL_PRIORITY_CLASS; break;
        case  2: priClass = HIGH_PRIORITY_CLASS;         break;
        default: return false;
    }

    HANDLE hProc = OpenProcess(PROCESS_SET_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!hProc) return false;
    BOOL ok = SetPriorityClass(hProc, priClass);
    CloseHandle(hProc);
    return ok != 0;
}

#endif // _WIN32
