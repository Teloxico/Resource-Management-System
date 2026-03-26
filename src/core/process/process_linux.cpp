/**
 * @file process_linux.cpp
 * @brief Linux process monitoring and management implementation.
 *
 * Enumerates /proc for numeric PID directories. For each PID reads
 * /proc/[pid]/stat, status, cmdline, and io to populate ProcessInfo.
 * CPU% is computed from utime+stime deltas in clock ticks.
 */

#ifdef __linux__

#include "process_linux.h"

#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <pwd.h>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cerrno>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

LinuxProcessManager::LinuxProcessManager() {
    clkTck_ = sysconf(_SC_CLK_TCK);
    if (clkTck_ <= 0) clkTck_ = 100;

    numProcessors_ = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (numProcessors_ < 1) numProcessors_ = 1;

    // Total physical memory.
    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        totalMemBytes_ = static_cast<uint64_t>(si.totalram)
                        * static_cast<uint64_t>(si.mem_unit);
    }
}

LinuxProcessManager::~LinuxProcessManager() = default;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Parse /proc/[pid]/stat.
 * Fields (1-indexed): pid (comm) state ppid ... utime(14) stime(15)
 *                     ... num_threads(20) ...
 * The comm field is enclosed in parentheses and may contain spaces,
 * so we locate the last ')' to find where comm ends.
 */
bool LinuxProcessManager::parseStat(int pid, ProcessInfo& info, CpuTicks& ticks) const {
    std::ifstream f("/proc/" + std::to_string(pid) + "/stat");
    if (!f.is_open()) return false;

    std::string line;
    if (!std::getline(f, line)) return false;

    // Find the name inside parentheses.
    auto openParen  = line.find('(');
    auto closeParen = line.rfind(')');
    if (openParen == std::string::npos || closeParen == std::string::npos)
        return false;

    info.name = line.substr(openParen + 1, closeParen - openParen - 1);

    // Everything after the closing paren, skip the space.
    std::istringstream ss(line.substr(closeParen + 2));

    // Fields after (comm): state(3) ppid(4) pgrp(5) session(6) tty_nr(7)
    // tpgid(8) flags(9) minflt(10) cminflt(11) majflt(12) cmajflt(13)
    // utime(14) stime(15) cutime(16) cstime(17) priority(18) nice(19)
    // num_threads(20)
    char state;
    int ppid;
    unsigned long long dummy;
    unsigned long long utime, stime;
    int priorityVal, niceVal, numThreads;

    ss >> state >> ppid;
    // Skip fields 5-13 (9 fields).
    for (int i = 0; i < 9; ++i) ss >> dummy;
    ss >> utime >> stime;
    // Skip cutime(16), cstime(17).
    ss >> dummy >> dummy;
    ss >> priorityVal >> niceVal >> numThreads;

    if (ss.fail()) return false;

    info.pid      = pid;
    info.state    = state;
    info.ppid     = ppid;
    info.priority = priorityVal;
    info.nice     = niceVal;
    info.threads  = numThreads;

    ticks.utime = utime;
    ticks.stime = stime;

    return true;
}

/**
 * Parse /proc/[pid]/status for VmRSS and Uid.
 */
bool LinuxProcessManager::parseStatus(int pid, ProcessInfo& info) const {
    std::ifstream f("/proc/" + std::to_string(pid) + "/status");
    if (!f.is_open()) return false;

    std::string line;
    bool gotRss = false, gotUid = false;

    while (std::getline(f, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            // Value is in kB.
            std::istringstream ss(line.substr(6));
            uint64_t rssKb = 0;
            ss >> rssKb;
            info.memoryBytes = rssKb * 1024ULL;
            if (totalMemBytes_ > 0) {
                info.memoryPercent = static_cast<float>(info.memoryBytes)
                                     / static_cast<float>(totalMemBytes_) * 100.0f;
            }
            gotRss = true;
        } else if (line.compare(0, 4, "Uid:") == 0) {
            std::istringstream ss(line.substr(4));
            unsigned int uid = 0;
            ss >> uid; // real UID
            info.user = uidToName(uid);
            gotUid = true;
        }
        if (gotRss && gotUid) break;
    }
    return true;
}

/**
 * Read /proc/[pid]/cmdline. Arguments are null-separated; join with spaces.
 */
std::string LinuxProcessManager::parseCmdline(int pid) const {
    std::ifstream f("/proc/" + std::to_string(pid) + "/cmdline",
                    std::ios::binary);
    if (!f.is_open()) return {};

    std::string raw((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());

    // Replace null bytes with spaces.
    for (auto& c : raw) {
        if (c == '\0') c = ' ';
    }
    // Trim trailing space.
    while (!raw.empty() && raw.back() == ' ')
        raw.pop_back();

    return raw;
}

/**
 * Parse /proc/[pid]/io for read_bytes and write_bytes (cumulative).
 * The caller computes rates from deltas.
 * May fail with EACCES for processes owned by other users.
 */
bool LinuxProcessManager::parseIo(int pid, IoBytes& ioOut) const {
    std::ifstream f("/proc/" + std::to_string(pid) + "/io");
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 12, "read_bytes: ") == 0) {
            try { ioOut.readBytes = std::stoll(line.substr(12)); } catch (...) {}
        } else if (line.compare(0, 13, "write_bytes: ") == 0) {
            try { ioOut.writeBytes = std::stoll(line.substr(13)); } catch (...) {}
        }
    }
    return true;
}

/**
 * Convert a numeric UID to a username via getpwuid().
 */
std::string LinuxProcessManager::uidToName(unsigned int uid) const {
    struct passwd* pw = getpwuid(uid);
    if (pw && pw->pw_name) {
        return pw->pw_name;
    }
    return std::to_string(uid);
}

// ---------------------------------------------------------------------------
// update()
// ---------------------------------------------------------------------------

void LinuxProcessManager::update() {
    ProcessSnapshot newSnap;
    std::unordered_map<int, CpuTicks> newTicks;
    std::unordered_map<int, IoBytes>  newIo;

    auto now = std::chrono::steady_clock::now();
    double wallDeltaSec = 0.0;
    if (hasPrevSample_) {
        wallDeltaSec = std::chrono::duration<double>(now - prevWall_).count();
    }

    int totalThreads     = 0;
    int runningProcesses = 0;

    DIR* procDir = opendir("/proc");
    if (!procDir) {
        return; // Cannot enumerate — keep stale snapshot.
    }

    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        // Only numeric directory names correspond to PIDs.
        const char* dname = entry->d_name;
        if (!std::isdigit(static_cast<unsigned char>(dname[0])))
            continue;

        // Verify all characters are digits.
        bool allDigits = true;
        for (const char* p = dname; *p; ++p) {
            if (!std::isdigit(static_cast<unsigned char>(*p))) {
                allDigits = false;
                break;
            }
        }
        if (!allDigits) continue;

        int pid = std::atoi(dname);
        if (pid <= 0) continue;

        ProcessInfo info;
        CpuTicks ticks;

        // Parse /proc/[pid]/stat (critical — skip if unavailable).
        if (!parseStat(pid, info, ticks))
            continue;

        // Parse /proc/[pid]/status for memory and user.
        parseStatus(pid, info);

        // Parse /proc/[pid]/cmdline.
        info.cmdline = parseCmdline(pid);

        // Parse /proc/[pid]/io and compute I/O rates from deltas.
        {
            IoBytes curIo;
            if (parseIo(pid, curIo)) {
                newIo[pid] = curIo;
                if (hasPrevSample_ && wallDeltaSec > 0.0) {
                    auto it = prevIo_.find(pid);
                    if (it != prevIo_.end()) {
                        int64_t dRead  = curIo.readBytes  - it->second.readBytes;
                        int64_t dWrite = curIo.writeBytes - it->second.writeBytes;
                        if (dRead  < 0) dRead  = 0;
                        if (dWrite < 0) dWrite = 0;
                        info.readBytesPerSec  = static_cast<int64_t>(
                            static_cast<double>(dRead) / wallDeltaSec);
                        info.writeBytesPerSec = static_cast<int64_t>(
                            static_cast<double>(dWrite) / wallDeltaSec);
                    }
                }
            }
        }

        // Path: use /proc/[pid]/exe symlink content (from cmdline first arg
        // as fallback is already in cmdline).
        {
            char buf[4096]{};
            std::string exeLink = "/proc/" + std::to_string(pid) + "/exe";
            ssize_t len = readlink(exeLink.c_str(), buf, sizeof(buf) - 1);
            if (len > 0) {
                buf[len] = '\0';
                info.path = buf;
            }
        }

        // CPU%.
        newTicks[pid] = ticks;
        if (hasPrevSample_ && wallDeltaSec > 0.0) {
            auto it = prevTicks_.find(pid);
            if (it != prevTicks_.end()) {
                unsigned long long dUtime = ticks.utime - it->second.utime;
                unsigned long long dStime = ticks.stime - it->second.stime;
                double cpuSec = static_cast<double>(dUtime + dStime)
                                / static_cast<double>(clkTck_);
                info.cpuPercent = static_cast<float>(
                    cpuSec / (wallDeltaSec * numProcessors_) * 100.0);
                if (info.cpuPercent < 0.0f)   info.cpuPercent = 0.0f;
                if (info.cpuPercent > 100.0f) info.cpuPercent = 100.0f;
            }
        }

        totalThreads += info.threads;
        if (info.state == 'R') ++runningProcesses;

        newSnap.processes.push_back(std::move(info));
    }
    closedir(procDir);

    newSnap.totalProcesses   = static_cast<int>(newSnap.processes.size());
    newSnap.totalThreads     = totalThreads;
    newSnap.runningProcesses = runningProcesses;

    // --- Swap into shared state ---
    {
        std::lock_guard<std::mutex> lock(mtx_);
        snap_       = std::move(newSnap);
        prevTicks_  = std::move(newTicks);
        prevIo_     = std::move(newIo);
        prevWall_   = now;
        hasPrevSample_ = true;
    }
}

// ---------------------------------------------------------------------------
// snapshot()
// ---------------------------------------------------------------------------

ProcessSnapshot LinuxProcessManager::snapshot() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return snap_;
}

// ---------------------------------------------------------------------------
// killProcess()
// ---------------------------------------------------------------------------

bool LinuxProcessManager::killProcess(int pid) {
    return (kill(pid, SIGTERM) == 0);
}

// ---------------------------------------------------------------------------
// setProcessPriority()
// ---------------------------------------------------------------------------

bool LinuxProcessManager::setProcessPriority(int pid, int niceValue) {
    // setpriority returns 0 on success, -1 on error.
    // Reset errno because -1 can also be a valid priority for getpriority().
    errno = 0;
    int ret = setpriority(PRIO_PROCESS, static_cast<id_t>(pid), niceValue);
    return (ret == 0);
}

#endif // __linux__
