/**
 * @file cpu_linux.h
 * @brief Linux implementation of the CPU monitoring interface.
 */

#pragma once

#ifdef __linux__

#include "cpu_common.h"

#include <vector>
#include <mutex>
#include <cstdint>
#include <chrono>

/**
 * @brief Linux CPU monitor using /proc/stat, /proc/cpuinfo, and sysfs.
 */
class LinuxCPU : public CPU {
public:
    LinuxCPU();
    ~LinuxCPU() override = default;

    /**
     * @brief Collect CPU metrics from procfs and sysfs.
     */
    void        update()                  override;

    /**
     * @brief Get a thread-safe copy of the latest snapshot.
     * @return Most recent CpuSnapshot.
     */
    CpuSnapshot snapshot() const          override;

private:
    /**
     * @brief Holds one sample of /proc/stat tick counters for a single CPU or aggregate.
     */
    struct CoreTick {
        uint64_t user    = 0; ///< Time in user mode
        uint64_t nice    = 0; ///< Time in user mode with low priority
        uint64_t system  = 0; ///< Time in kernel mode
        uint64_t idle    = 0; ///< Idle time
        uint64_t iowait  = 0; ///< Time waiting for I/O
        uint64_t irq     = 0; ///< Time servicing hardware interrupts
        uint64_t softirq = 0; ///< Time servicing soft interrupts
        uint64_t steal   = 0; ///< Time stolen by hypervisor

        /**
         * @brief Sum of all tick fields.
         * @return Total ticks.
         */
        uint64_t total() const {
            return user + nice + system + idle + iowait + irq + softirq + steal;
        }

        /**
         * @brief Total ticks minus idle and iowait.
         * @return Active (non-idle) ticks.
         */
        uint64_t activeTime() const { return total() - idle - iowait; }
    };

    CoreTick prevAgg_; ///< Previous aggregate tick values
    std::vector<CoreTick> prevCores_; ///< Previous per-core tick values

    uint64_t prevCtx_       = 0; ///< Previous context switch count
    uint64_t prevIntr_      = 0; ///< Previous interrupt count
    std::chrono::steady_clock::time_point prevTime_; ///< Timestamp of last update

    int logicalCores_ = 0; ///< Number of online logical CPUs

    static constexpr size_t kMaxHistory = 300; ///< Max stored usage samples
    std::vector<float> usageHistory_; ///< Rolling CPU usage history

    mutable std::mutex mutex_; ///< Guards current_
    CpuSnapshot        current_; ///< Latest snapshot

    /**
     * @brief Compute CPU usage percentage between two tick samples.
     * @param prev Previous tick sample.
     * @param cur Current tick sample.
     * @return Usage as a percentage (0-100).
     */
    static float computeUsage(const CoreTick& prev, const CoreTick& cur);

    /**
     * @brief Read CPU temperature from /sys/class/hwmon.
     * @return Temperature in Celsius, or -1 on failure.
     */
    float        readTemperature() const;
};

#endif // __linux__
