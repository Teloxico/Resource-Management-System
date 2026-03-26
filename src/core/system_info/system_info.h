/**
 * @file system_info.h
 * @brief Collects static and dynamic system information.
 *
 * Unlike other modules, SystemInfo is NOT abstract -- it uses #ifdef
 * internally to select platform-specific code, since most queries are
 * simple one-liners. Static fields (OS name, CPU model, core counts, etc.)
 * are queried once in the constructor; dynamic fields (uptime) are
 * refreshed on each update() call.
 */

#pragma once

#include "../metrics.h"

#include <mutex>

class SystemInfo {
public:
    SystemInfo();

    /**
     * @brief Refresh dynamic fields (currently just uptime).
     *
     * Called periodically by the collector thread.
     */
    void update();

    /**
     * @brief Return a thread-safe copy of the current system information.
     */
    SystemInfoSnapshot snapshot() const;

private:
    mutable std::mutex mtx_;
    SystemInfoSnapshot data_;

    /**
     * @brief Query static fields (called once in the constructor).
     *
     * Populates osName, osVersion, kernelVersion, hostname, arch,
     * cpuModel, cpuPhysicalCores, cpuLogicalCores, totalRAM.
     */
    void queryStatic();

    /**
     * @brief Query dynamic fields (called in update()).
     *
     * Currently refreshes uptimeSeconds.
     */
    void queryDynamic();
};
