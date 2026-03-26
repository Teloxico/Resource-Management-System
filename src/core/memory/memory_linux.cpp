/**
 * @file memory_linux.cpp
 * @brief Linux memory monitoring using /proc/meminfo, /proc/vmstat, and per-process status files.
 */

#ifdef __linux__

#include "memory_linux.h"

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <cctype>
#include <dirent.h>
#include <filesystem>

namespace fs = std::filesystem;

LinuxMemory::LinuxMemory()
    : lastProcessScan_(std::chrono::steady_clock::now() - std::chrono::seconds(kProcessScanIntervalSec + 1))
    , prevTime_(std::chrono::steady_clock::now())
{
    std::ifstream vmstat("/proc/vmstat");
    if (vmstat.is_open()) {
        std::string key;
        uint64_t val = 0;
        while (vmstat >> key >> val) {
            if (key == "pgfault") {
                prevPgFault_ = val;
                break;
            }
        }
    }

    usageHistory_.reserve(kMaxHistory);
}

/**
 * @brief Scan /proc/[pid]/status for the top 5 processes by VmRSS.
 * @param outName  Receives the name of the highest-RSS process.
 * @param outMem   Receives its RSS in bytes.
 * @param topProcs Receives up to 5 top processes sorted descending by RSS.
 */
void LinuxMemory::scanTopProcess(std::string& outName, uint64_t& outMem,
                                 std::vector<MemorySnapshot::TopProcess>& topProcs)
{
    struct ProcEntry {
        std::string name;
        uint64_t    rssKb = 0;
    };
    std::vector<ProcEntry> entries;

    try {
        for (const auto& entry : fs::directory_iterator("/proc")) {
            if (!entry.is_directory()) continue;

            const std::string fname = entry.path().filename().string();
            if (fname.empty() || !std::isdigit(static_cast<unsigned char>(fname[0])))
                continue;

            std::ifstream status(entry.path() / "status");
            if (!status.is_open()) continue;

            ProcEntry pe;
            std::string line;

            while (std::getline(status, line)) {
                if (line.compare(0, 5, "Name:") == 0) {
                    pe.name = line.substr(5);
                    auto it = pe.name.begin();
                    while (it != pe.name.end() && (*it == ' ' || *it == '\t'))
                        ++it;
                    pe.name.erase(pe.name.begin(), it);
                } else if (line.compare(0, 6, "VmRSS:") == 0) {
                    std::istringstream ss(line.substr(6));
                    ss >> pe.rssKb;
                    break;
                }
            }

            if (pe.rssKb > 0)
                entries.push_back(std::move(pe));
        }
    } catch (...) {
    }

    std::sort(entries.begin(), entries.end(),
              [](const ProcEntry& a, const ProcEntry& b) { return a.rssKb > b.rssKb; });

    if (!entries.empty()) {
        outName = entries[0].name;
        outMem  = entries[0].rssKb * 1024ULL;
    } else {
        outName = "Unknown";
        outMem  = 0;
    }

    topProcs.clear();
    size_t limit = std::min(entries.size(), static_cast<size_t>(5));
    for (size_t i = 0; i < limit; ++i) {
        MemorySnapshot::TopProcess tp;
        tp.name        = entries[i].name;
        tp.memoryBytes = entries[i].rssKb * 1024ULL;
        topProcs.push_back(std::move(tp));
    }
}

void LinuxMemory::update() {
    MemorySnapshot snap;

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - prevTime_).count();
    if (elapsed <= 0.0) elapsed = 1.0;

    {
        std::ifstream meminfo("/proc/meminfo");
        if (meminfo.is_open()) {
            uint64_t memTotal      = 0;
            uint64_t memAvailable  = 0;
            uint64_t memFree       = 0;
            uint64_t buffers       = 0;
            uint64_t cached        = 0;
            uint64_t swapTotal     = 0;
            uint64_t swapFree      = 0;
            uint64_t committedAS   = 0;
            uint64_t commitLimit   = 0;
            uint64_t slab          = 0;
            uint64_t sReclaimable  = 0;

            std::string line;
            while (std::getline(meminfo, line)) {
                std::istringstream ss(line);
                std::string key;
                uint64_t    val = 0;
                ss >> key >> val;

                if      (key == "MemTotal:")      memTotal     = val;
                else if (key == "MemAvailable:")  memAvailable = val;
                else if (key == "MemFree:")       memFree      = val;
                else if (key == "Buffers:")       buffers      = val;
                else if (key == "Cached:")        cached       = val;
                else if (key == "SwapTotal:")     swapTotal    = val;
                else if (key == "SwapFree:")      swapFree     = val;
                else if (key == "Committed_AS:")  committedAS  = val;
                else if (key == "CommitLimit:")   commitLimit  = val;
                else if (key == "Slab:")          slab         = val;
                else if (key == "SReclaimable:")  sReclaimable = val;
            }

            constexpr uint64_t KB = 1024ULL;

            snap.totalBytes     = memTotal     * KB;
            snap.availableBytes = memAvailable * KB;
            snap.usedBytes      = (memTotal - memAvailable) * KB;
            snap.cachedBytes    = (cached + sReclaimable)   * KB;
            snap.bufferedBytes  = buffers * KB;

            snap.swapTotal  = swapTotal * KB;
            snap.swapFree   = swapFree  * KB;
            snap.swapUsed   = (swapTotal >= swapFree) ? (swapTotal - swapFree) * KB : 0;
            snap.swapPercent = (swapTotal > 0)
                ? static_cast<float>(swapTotal - swapFree) * 100.0f
                  / static_cast<float>(swapTotal)
                : 0.0f;

            snap.committedBytes   = committedAS * KB;
            snap.commitLimitBytes = commitLimit  * KB;

            if (memTotal > 0) {
                snap.usagePercent = static_cast<float>(memTotal - memAvailable) * 100.0f
                                    / static_cast<float>(memTotal);
            }
        }
    }

    {
        std::ifstream vmstat("/proc/vmstat");
        if (vmstat.is_open()) {
            std::string key;
            uint64_t    val = 0;
            while (vmstat >> key >> val) {
                if (key == "pgfault") {
                    if (elapsed > 0.0 && val >= prevPgFault_) {
                        snap.pageFaultsPerSec = static_cast<float>(
                            static_cast<double>(val - prevPgFault_) / elapsed);
                    }
                    prevPgFault_ = val;
                    break;
                }
            }
        }
    }

    prevTime_ = now;

    {
        auto secsSinceScan = std::chrono::duration_cast<std::chrono::seconds>(
            now - lastProcessScan_).count();
        if (secsSinceScan >= kProcessScanIntervalSec) {
            scanTopProcess(cachedTopName_, cachedTopMem_, cachedTopProcs_);
            lastProcessScan_ = now;
        }
        snap.topProcessName   = cachedTopName_;
        snap.topProcessMemory = cachedTopMem_;
        snap.topProcesses     = cachedTopProcs_;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);

        usageHistory_.push_back(snap.usagePercent);
        if (usageHistory_.size() > kMaxHistory)
            usageHistory_.erase(usageHistory_.begin());

        if (!usageHistory_.empty()) {
            float sum = std::accumulate(usageHistory_.begin(), usageHistory_.end(), 0.0f);
            snap.averageUsage = sum / static_cast<float>(usageHistory_.size());
        }

        current_ = std::move(snap);
    }
}

/**
 * @brief Return current snapshot under lock.
 * @return Copy of the latest MemorySnapshot.
 */
MemorySnapshot LinuxMemory::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

#endif
