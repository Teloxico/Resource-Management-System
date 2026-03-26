/**
 * @file memory_windows.cpp
 * @brief Windows memory monitoring using Win32 and PDH APIs.
 */

#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "memory_windows.h"

#include <algorithm>
#include <numeric>
#include <vector>

WindowsMemory::WindowsMemory()
    : lastProcessScan_(std::chrono::steady_clock::now() - std::chrono::seconds(kProcessScanIntervalSec + 1))
    , prevTime_(std::chrono::steady_clock::now())
{
    usageHistory_.reserve(kMaxHistory);

    if (PdhOpenQueryA(nullptr, 0, &pdhQuery_) == ERROR_SUCCESS) {
        bool ok = true;
        ok = ok && (PdhAddEnglishCounterA(pdhQuery_,
                    "\\Memory\\Page Faults/sec",
                    0, &pageFaultCounter_) == ERROR_SUCCESS);
        ok = ok && (PdhAddEnglishCounterA(pdhQuery_,
                    "\\Memory\\Cache Bytes",
                    0, &cacheBytesCounter_) == ERROR_SUCCESS);
        if (ok) {
            PdhCollectQueryData(pdhQuery_);
            firstCollect_ = true;
            pdhReady_ = true;
        } else {
            PdhCloseQuery(pdhQuery_);
            pdhQuery_ = nullptr;
        }
    }
}

WindowsMemory::~WindowsMemory() {
    if (pdhQuery_) {
        PdhCloseQuery(pdhQuery_);
        pdhQuery_ = nullptr;
    }
}

/**
 * @brief Enumerate processes and pick the top 5 by working set size.
 * @param outName  Receives name of the highest-memory process.
 * @param outMem   Receives its working set in bytes.
 * @param topProcs Receives up to 5 top processes sorted by memory.
 */
void WindowsMemory::scanTopProcess(std::string& outName, uint64_t& outMem,
                                   std::vector<MemorySnapshot::TopProcess>& topProcs)
{
    DWORD pids[4096];
    DWORD cbNeeded = 0;
    if (!EnumProcesses(pids, sizeof(pids), &cbNeeded))
        return;

    DWORD count = cbNeeded / sizeof(DWORD);

    struct ProcEntry {
        std::string name;
        SIZE_T      ws = 0;
    };
    std::vector<ProcEntry> entries;
    entries.reserve(count);

    for (DWORD i = 0; i < count; ++i) {
        if (pids[i] == 0) continue;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                   FALSE, pids[i]);
        if (!hProc) continue;

        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
            ProcEntry e;
            e.ws = pmc.WorkingSetSize;

            char nameBuf[MAX_PATH] = {};
            HMODULE hMod = nullptr;
            DWORD cbMod  = 0;
            if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &cbMod)) {
                GetModuleBaseNameA(hProc, hMod, nameBuf, sizeof(nameBuf));
            }
            e.name = (nameBuf[0] != '\0') ? nameBuf : "Unknown";
            entries.push_back(std::move(e));
        }
        CloseHandle(hProc);
    }

    std::sort(entries.begin(), entries.end(),
              [](const ProcEntry& a, const ProcEntry& b) { return a.ws > b.ws; });

    if (!entries.empty()) {
        outName = entries[0].name;
        outMem  = static_cast<uint64_t>(entries[0].ws);
    } else {
        outName = "Unknown";
        outMem  = 0;
    }

    topProcs.clear();
    size_t limit = std::min(entries.size(), static_cast<size_t>(5));
    for (size_t i = 0; i < limit; ++i) {
        MemorySnapshot::TopProcess tp;
        tp.name        = entries[i].name;
        tp.memoryBytes = static_cast<uint64_t>(entries[i].ws);
        topProcs.push_back(std::move(tp));
    }
}

void WindowsMemory::update() {
    MemorySnapshot snap;

    auto now = std::chrono::steady_clock::now();

    MEMORYSTATUSEX msx{};
    msx.dwLength = sizeof(msx);
    if (GlobalMemoryStatusEx(&msx)) {
        snap.totalBytes     = msx.ullTotalPhys;
        snap.availableBytes = msx.ullAvailPhys;
        snap.usedBytes      = msx.ullTotalPhys - msx.ullAvailPhys;
        snap.usagePercent   = static_cast<float>(msx.dwMemoryLoad);

        uint64_t pageTotal = msx.ullTotalPageFile;
        uint64_t pageAvail = msx.ullAvailPageFile;
        snap.swapTotal = (pageTotal > msx.ullTotalPhys) ? (pageTotal - msx.ullTotalPhys) : 0;
        uint64_t pageUsed = pageTotal - pageAvail;
        uint64_t physUsed = msx.ullTotalPhys - msx.ullAvailPhys;
        snap.swapUsed  = (pageUsed > physUsed) ? (pageUsed - physUsed) : 0;
        snap.swapFree  = (snap.swapTotal > snap.swapUsed) ? (snap.swapTotal - snap.swapUsed) : 0;
        snap.swapPercent = (snap.swapTotal > 0)
            ? static_cast<float>(snap.swapUsed) * 100.0f / static_cast<float>(snap.swapTotal)
            : 0.0f;
    }

    PERFORMANCE_INFORMATION pi{};
    pi.cb = sizeof(pi);
    if (GetPerformanceInfo(&pi, sizeof(pi))) {
        SIZE_T pageSize = pi.PageSize;
        snap.committedBytes    = static_cast<uint64_t>(pi.CommitTotal)      * pageSize;
        snap.commitLimitBytes  = static_cast<uint64_t>(pi.CommitLimit)      * pageSize;
        snap.pagedPoolBytes    = static_cast<uint64_t>(pi.KernelPaged)      * pageSize;
        snap.nonPagedPoolBytes = static_cast<uint64_t>(pi.KernelNonpaged)   * pageSize;
    }

    if (pdhReady_ && pdhQuery_) {
        if (PdhCollectQueryData(pdhQuery_) == ERROR_SUCCESS && !firstCollect_) {
            if (pageFaultCounter_) {
                PDH_FMT_COUNTERVALUE val{};
                if (PdhGetFormattedCounterValue(pageFaultCounter_, PDH_FMT_DOUBLE,
                                                nullptr, &val) == ERROR_SUCCESS) {
                    snap.pageFaultsPerSec = static_cast<float>(val.doubleValue);
                }
            }
            if (cacheBytesCounter_) {
                PDH_FMT_COUNTERVALUE val{};
                if (PdhGetFormattedCounterValue(cacheBytesCounter_, PDH_FMT_DOUBLE,
                                                nullptr, &val) == ERROR_SUCCESS) {
                    snap.cachedBytes = static_cast<uint64_t>(val.doubleValue);
                }
            }
        }
        firstCollect_ = false;
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
MemorySnapshot WindowsMemory::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

#endif
