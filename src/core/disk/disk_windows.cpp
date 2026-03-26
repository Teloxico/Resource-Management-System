/**
 * @file disk_windows.cpp
 * @brief Windows disk monitoring via Win32 drive APIs and PDH counters.
 */

#ifdef _WIN32

#include "disk_windows.h"

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "pdh.lib")

WindowsDisk::WindowsDisk() {
    if (PdhOpenQueryA(nullptr, 0, &pdhQuery_) != ERROR_SUCCESS) {
        pdhQuery_ = nullptr;
        return;
    }

    bool ok = true;
    ok = ok && (PdhAddEnglishCounterA(pdhQuery_,
                "\\PhysicalDisk(_Total)\\Disk Read Bytes/sec",
                0, &readBytesCounter_) == ERROR_SUCCESS);
    ok = ok && (PdhAddEnglishCounterA(pdhQuery_,
                "\\PhysicalDisk(_Total)\\Disk Write Bytes/sec",
                0, &writeBytesCounter_) == ERROR_SUCCESS);
    ok = ok && (PdhAddEnglishCounterA(pdhQuery_,
                "\\PhysicalDisk(_Total)\\Disk Reads/sec",
                0, &readOpsCounter_) == ERROR_SUCCESS);
    ok = ok && (PdhAddEnglishCounterA(pdhQuery_,
                "\\PhysicalDisk(_Total)\\Disk Writes/sec",
                0, &writeOpsCounter_) == ERROR_SUCCESS);
    ok = ok && (PdhAddEnglishCounterA(pdhQuery_,
                "\\PhysicalDisk(_Total)\\% Disk Time",
                0, &diskTimeCounter_) == ERROR_SUCCESS);

    if (!ok) {
        PdhCloseQuery(pdhQuery_);
        pdhQuery_ = nullptr;
        return;
    }

    enumeratePdhInstances();

    PdhCollectQueryData(pdhQuery_);
    firstCollect_ = true;
    pdhReady_     = true;
}

WindowsDisk::~WindowsDisk() {
    if (pdhQuery_) {
        PdhCloseQuery(pdhQuery_);
        pdhQuery_ = nullptr;
    }
}

void WindowsDisk::enumeratePdhInstances() {
    if (perDiskEnumerated_ || !pdhQuery_) return;
    perDiskEnumerated_ = true;

    DWORD counterListSize  = 0;
    DWORD instanceListSize = 0;
    PdhEnumObjectItemsA(nullptr, nullptr, "PhysicalDisk",
                        nullptr, &counterListSize,
                        nullptr, &instanceListSize,
                        PERF_DETAIL_WIZARD, 0);
    if (instanceListSize == 0) return;

    std::vector<char> instanceBuf(instanceListSize);
    std::vector<char> counterBuf(counterListSize);
    if (PdhEnumObjectItemsA(nullptr, nullptr, "PhysicalDisk",
                            counterBuf.data(), &counterListSize,
                            instanceBuf.data(), &instanceListSize,
                            PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS) {
        return;
    }

    const char* p = instanceBuf.data();
    while (*p) {
        std::string inst(p);
        p += inst.size() + 1;

        if (inst == "_Total") continue;

        PdhDiskCounters dc;
        dc.instanceName = inst;

        std::string prefix = "\\PhysicalDisk(" + inst + ")\\";
        PdhAddEnglishCounterA(pdhQuery_, (prefix + "Disk Read Bytes/sec").c_str(), 0, &dc.readBytes);
        PdhAddEnglishCounterA(pdhQuery_, (prefix + "Disk Write Bytes/sec").c_str(), 0, &dc.writeBytes);
        PdhAddEnglishCounterA(pdhQuery_, (prefix + "Disk Reads/sec").c_str(), 0, &dc.readOps);
        PdhAddEnglishCounterA(pdhQuery_, (prefix + "Disk Writes/sec").c_str(), 0, &dc.writeOps);
        PdhAddEnglishCounterA(pdhQuery_, (prefix + "% Disk Time").c_str(), 0, &dc.diskTime);

        perDiskCounters_.push_back(std::move(dc));
    }
}

void WindowsDisk::update() {
    DiskSnapshot snap;

    queryDriveSpace(snap);
    queryIOCounters(snap);

    std::lock_guard<std::mutex> lock(mutex_);
    current_ = std::move(snap);
}

DiskSnapshot WindowsDisk::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

void WindowsDisk::queryDriveSpace(DiskSnapshot& snap) {
    char driveBuf[512];
    DWORD len = GetLogicalDriveStringsA(sizeof(driveBuf) - 1, driveBuf);
    if (len == 0 || len > sizeof(driveBuf) - 1) return;

    const char* p = driveBuf;
    while (*p) {
        std::string root(p);
        p += root.size() + 1;

        UINT driveType = GetDriveTypeA(root.c_str());
        if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE)
            continue;

        DiskInfo info;
        info.device     = root.substr(0, 2);
        info.mountPoint = root;

        char fsName[64] = {};
        if (GetVolumeInformationA(root.c_str(), nullptr, 0,
                                  nullptr, nullptr, nullptr,
                                  fsName, sizeof(fsName))) {
            info.fsType = fsName;
        }

        ULARGE_INTEGER freeBytesAvail, totalBytes, totalFreeBytes;
        if (GetDiskFreeSpaceExA(root.c_str(),
                                &freeBytesAvail, &totalBytes, &totalFreeBytes)) {
            info.totalBytes = totalBytes.QuadPart;
            info.freeBytes  = totalFreeBytes.QuadPart;
            info.usedBytes  = info.totalBytes - info.freeBytes;
            if (info.totalBytes > 0)
                info.usagePercent = static_cast<float>(info.usedBytes) * 100.0f
                                  / static_cast<float>(info.totalBytes);
        }

        info.temperature = -1.0f;
        snap.disks.push_back(std::move(info));
    }
}

void WindowsDisk::queryIOCounters(DiskSnapshot& snap) {
    if (!pdhReady_ || !pdhQuery_) return;

    if (PdhCollectQueryData(pdhQuery_) != ERROR_SUCCESS) return;

    if (firstCollect_) {
        firstCollect_ = false;
        return;
    }

    auto getDouble = [](PDH_HCOUNTER counter) -> double {
        if (!counter) return 0.0;
        PDH_FMT_COUNTERVALUE val;
        if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS)
            return val.doubleValue;
        return 0.0;
    };

    bool assignedPerDisk = false;

    for (const auto& dc : perDiskCounters_) {
        float readBps  = static_cast<float>(getDouble(dc.readBytes));
        float writeBps = static_cast<float>(getDouble(dc.writeBytes));
        float readOps  = static_cast<float>(getDouble(dc.readOps));
        float writeOps = static_cast<float>(getDouble(dc.writeOps));
        float diskTime = static_cast<float>(getDouble(dc.diskTime));
        if (diskTime > 100.0f) diskTime = 100.0f;

        for (auto& disk : snap.disks) {
            if (dc.instanceName.find(disk.device) != std::string::npos) {
                disk.readBytesPerSec  = readBps;
                disk.writeBytesPerSec = writeBps;
                disk.readOpsPerSec    = readOps;
                disk.writeOpsPerSec   = writeOps;
                disk.utilizationPct   = diskTime;
                assignedPerDisk = true;
            }
        }
    }

    float totalReadBps  = static_cast<float>(getDouble(readBytesCounter_));
    float totalWriteBps = static_cast<float>(getDouble(writeBytesCounter_));

    snap.totalReadRate  = totalReadBps;
    snap.totalWriteRate = totalWriteBps;

    if (!assignedPerDisk && !snap.disks.empty()) {
        float totalReadOps  = static_cast<float>(getDouble(readOpsCounter_));
        float totalWriteOps = static_cast<float>(getDouble(writeOpsCounter_));
        float totalDiskTime = static_cast<float>(getDouble(diskTimeCounter_));
        if (totalDiskTime > 100.0f) totalDiskTime = 100.0f;

        for (auto& d : snap.disks) {
            d.readBytesPerSec  = totalReadBps;
            d.writeBytesPerSec = totalWriteBps;
            d.readOpsPerSec    = totalReadOps;
            d.writeOpsPerSec   = totalWriteOps;
            d.utilizationPct   = totalDiskTime;
        }
    }
}

#endif
