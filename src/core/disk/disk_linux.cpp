/**
 * @file disk_linux.cpp
 * @brief Linux disk monitoring via /proc/mounts, statvfs, and /proc/diskstats.
 */

#ifdef __linux__

#include "disk_linux.h"

#include <sys/statvfs.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr uint64_t SECTOR_SIZE = 512;

LinuxDisk::LinuxDisk()
    : prevTime_(std::chrono::steady_clock::now())
{
    prevStats_ = readDiskStats();
}

void LinuxDisk::update() {
    DiskSnapshot snap;

    auto mounts = readMounts();
    for (const auto& m : mounts) {
        struct statvfs vfs {};
        if (statvfs(m.mountPoint.c_str(), &vfs) != 0) {
            continue;
        }

        DiskInfo info;
        info.device     = m.device;
        info.mountPoint = m.mountPoint;
        info.fsType     = m.fsType;

        uint64_t blockSize = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
        info.totalBytes = static_cast<uint64_t>(vfs.f_blocks) * blockSize;
        info.freeBytes  = static_cast<uint64_t>(vfs.f_bfree)  * blockSize;
        info.usedBytes  = info.totalBytes - info.freeBytes;
        if (info.totalBytes > 0) {
            info.usagePercent = static_cast<float>(info.usedBytes) * 100.0f
                              / static_cast<float>(info.totalBytes);
        }
        info.temperature = -1.0f;

        snap.disks.push_back(std::move(info));
    }

    auto now      = std::chrono::steady_clock::now();
    auto elapsed  = std::chrono::duration_cast<std::chrono::milliseconds>(now - prevTime_);
    double dtMs   = static_cast<double>(elapsed.count());
    if (dtMs <= 0.0) dtMs = 1.0;

    auto curStats = readDiskStats();

    std::unordered_map<std::string, DiskInfo*> byName;
    for (auto& d : snap.disks) {
        std::string shortName = baseDeviceName(d.device);
        byName[shortName] = &d;
    }

    float totalRead  = 0.0f;
    float totalWrite = 0.0f;

    for (const auto& [name, cur] : curStats) {
        auto pit = prevStats_.find(name);
        if (pit == prevStats_.end()) continue;
        const DiskStats& prev = pit->second;

        uint64_t dReads   = cur.readsCompleted  - prev.readsCompleted;
        uint64_t dWrites  = cur.writesCompleted - prev.writesCompleted;
        uint64_t dSecRead = cur.sectorsRead     - prev.sectorsRead;
        uint64_t dSecWrt  = cur.sectorsWritten  - prev.sectorsWritten;
        uint64_t dIo      = cur.ioTicks         - prev.ioTicks;

        float readBps  = static_cast<float>(dSecRead * SECTOR_SIZE) * 1000.0f
                       / static_cast<float>(dtMs);
        float writeBps = static_cast<float>(dSecWrt  * SECTOR_SIZE) * 1000.0f
                       / static_cast<float>(dtMs);
        float readOps  = static_cast<float>(dReads)  * 1000.0f / static_cast<float>(dtMs);
        float writeOps = static_cast<float>(dWrites) * 1000.0f / static_cast<float>(dtMs);
        float util     = static_cast<float>(dIo) * 100.0f / static_cast<float>(dtMs);
        if (util > 100.0f) util = 100.0f;

        auto it = byName.find(name);
        if (it != byName.end()) {
            DiskInfo* d = it->second;
            d->readBytesPerSec  = readBps;
            d->writeBytesPerSec = writeBps;
            d->readOpsPerSec    = readOps;
            d->writeOpsPerSec   = writeOps;
            d->utilizationPct   = util;
        }

        if (isRealDiskName(name)) {
            totalRead  += readBps;
            totalWrite += writeBps;
        }
    }

    snap.totalReadRate  = totalRead;
    snap.totalWriteRate = totalWrite;

    prevStats_ = std::move(curStats);
    prevTime_  = now;

    std::lock_guard<std::mutex> lock(mutex_);
    current_ = std::move(snap);
}

DiskSnapshot LinuxDisk::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

std::vector<LinuxDisk::MountEntry> LinuxDisk::readMounts() const {
    std::vector<MountEntry> results;
    std::ifstream fin("/proc/mounts");
    if (!fin.is_open()) return results;

    std::string line;
    while (std::getline(fin, line)) {
        std::istringstream ss(line);
        MountEntry e;
        std::string opts;
        int dump = 0, pass = 0;
        if (!(ss >> e.device >> e.mountPoint >> e.fsType)) continue;

        if (!isRealDevice(e.device)) continue;

        results.push_back(std::move(e));
    }
    return results;
}

std::unordered_map<std::string, LinuxDisk::DiskStats>
LinuxDisk::readDiskStats() const {
    std::unordered_map<std::string, DiskStats> result;
    std::ifstream fin("/proc/diskstats");
    if (!fin.is_open()) return result;

    std::string line;
    while (std::getline(fin, line)) {
        std::istringstream ss(line);
        int major = 0, minor = 0;
        std::string name;
        if (!(ss >> major >> minor >> name)) continue;

        if (!isRealDiskName(name) && name.find("sd") == std::string::npos &&
            name.find("nvme") == std::string::npos && name.find("vd") == std::string::npos) {
            continue;
        }

        DiskStats ds;
        uint64_t ignore;
        if (!(ss >> ds.readsCompleted >> ignore >> ds.sectorsRead >> ignore
                 >> ds.writesCompleted >> ignore >> ds.sectorsWritten >> ignore
                 >> ignore >> ds.ioTicks)) {
            continue;
        }
        result[name] = ds;
    }
    return result;
}

std::string LinuxDisk::baseDeviceName(const std::string& device) {
    auto pos = device.rfind('/');
    if (pos != std::string::npos) return device.substr(pos + 1);
    return device;
}

bool LinuxDisk::isRealDevice(const std::string& devPath) {
    if (devPath.rfind("/dev/sd",   0) == 0) return true;
    if (devPath.rfind("/dev/nvme", 0) == 0) return true;
    if (devPath.rfind("/dev/vd",   0) == 0) return true;
    if (devPath.rfind("/dev/xvd",  0) == 0) return true;
    if (devPath.rfind("/dev/hd",   0) == 0) return true;
    return false;
}

bool LinuxDisk::isRealDiskName(const std::string& name) {
    if (name.rfind("sd",   0) == 0 && name.size() <= 3) return true;
    if (name.rfind("vd",   0) == 0 && name.size() <= 3) return true;
    if (name.rfind("xvd",  0) == 0 && name.size() <= 4) return true;
    if (name.rfind("hd",   0) == 0 && name.size() <= 3) return true;
    if (name.rfind("nvme", 0) == 0 && name.find('p') == std::string::npos) return true;
    return false;
}

#endif
