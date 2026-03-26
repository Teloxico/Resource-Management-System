/**
 * @file disk_common.h
 * @brief Abstract base class and factory declaration for disk monitoring.
 */

#pragma once

#include "../metrics.h"
#include <memory>

/**
 * @brief Abstract interface for platform-specific disk monitors.
 */
class Disk {
public:
    virtual ~Disk() = default;

    /**
     * @brief Collect current disk metrics from the OS.
     */
    virtual void update() = 0;

    /**
     * @brief Get a thread-safe copy of the latest disk metrics.
     * @return Most recent DiskSnapshot.
     */
    virtual DiskSnapshot snapshot() const = 0;
};

/**
 * @brief Create a platform-specific Disk implementation.
 * @return Owning pointer to the concrete Disk instance.
 */
std::unique_ptr<Disk> createDisk();
