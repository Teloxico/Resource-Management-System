/**
 * @file disk_linux.h
 * @brief Linux disk monitoring implementation using /proc and statvfs.
 */

#pragma once

#ifdef __linux__

#include "disk_common.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Linux disk monitor reading /proc/mounts, statvfs, and /proc/diskstats.
 */
class LinuxDisk : public Disk {
public:
    LinuxDisk();
    ~LinuxDisk() override = default;

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
     * @brief Raw I/O counters from one /proc/diskstats entry.
     */
    struct DiskStats {
        uint64_t readsCompleted  = 0; ///< Completed read operations
        uint64_t sectorsRead     = 0; ///< Sectors read (512 bytes each)
        uint64_t writesCompleted = 0; ///< Completed write operations
        uint64_t sectorsWritten  = 0; ///< Sectors written (512 bytes each)
        uint64_t ioTicks         = 0; ///< Milliseconds spent doing I/O
    };

    /**
     * @brief One entry parsed from /proc/mounts.
     */
    struct MountEntry {
        std::string device;     ///< Device path, e.g. /dev/sda1
        std::string mountPoint; ///< Mount point path
        std::string fsType;     ///< Filesystem type, e.g. ext4
    };

    /**
     * @brief Parse /proc/mounts for real block devices.
     * @return List of mount entries.
     */
    std::vector<MountEntry> readMounts() const;

    /**
     * @brief Parse /proc/diskstats for I/O counters.
     * @return Map of device name to raw I/O stats.
     */
    std::unordered_map<std::string, DiskStats> readDiskStats() const;

    /**
     * @brief Extract short device name from a full path (e.g. "/dev/sda1" -> "sda1").
     * @param device Full device path.
     * @return Short device name.
     */
    static std::string baseDeviceName(const std::string& device);

    /**
     * @brief Check if a device path refers to a real block device.
     * @param devPath Device path to check.
     * @return True if it is a recognized block device.
     */
    static bool isRealDevice(const std::string& devPath);

    /**
     * @brief Check if a name is a whole-disk device (not a partition).
     * @param name Short device name.
     * @return True for whole-disk names like sda, nvme0n1.
     */
    static bool isRealDiskName(const std::string& name);

    std::unordered_map<std::string, DiskStats> prevStats_; ///< Previous tick stats for delta computation
    std::chrono::steady_clock::time_point      prevTime_;  ///< Timestamp of previous tick

    mutable std::mutex mutex_;   ///< Protects current_
    DiskSnapshot       current_; ///< Latest snapshot
};

#endif
