// File: src/core/memory/memory_linux.h

#ifndef MEMORY_LINUX_H
#define MEMORY_LINUX_H

#ifdef __linux__

#include "memory_common.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>

/**
 * @class LinuxMemory
 * @brief Linux-specific implementation for Memory monitoring.
 *
 * Utilizes /proc/meminfo and /proc/[PID]/status to monitor memory usage.
 */
class LinuxMemory : public Memory {
public:
    /**
     * @brief Constructs a new LinuxMemory object and initializes memory statistics.
     */
    LinuxMemory();

    /**
     * @brief Destructs the LinuxMemory object.
     *
     * Ensures that all resources are properly released.
     */
    ~LinuxMemory() override;

    /**
     * @brief Retrieves the latest total memory usage percentage.
     * @return Total memory usage as a percentage.
     */
    float getTotalUsage() override;

    /**
     * @brief Retrieves the remaining RAM in megabytes.
     * @return Remaining RAM in MB.
     */
    float getRemainingRAM() override;

    /**
     * @brief Retrieves the average memory usage percentage over the history.
     * @return Average memory usage as a percentage.
     */
    float getAverageUsage() override;

    /**
     * @brief Retrieves the name of the process consuming the most memory.
     * @return Name of the top memory-consuming process.
     */
    std::string getMostUsingProcess() override;

private:
    /**
     * @brief Reads memory information from /proc/meminfo and updates member variables.
     * @return true If memory information was successfully read.
     * @return false If there was an error reading memory information.
     */
    bool readMemInfo();

    /**
     * @brief Reads process memory usage from /proc/[PID]/status and identifies the top memory-consuming process.
     * @return true If process memory usage was successfully read.
     * @return false If there was an error reading process memory usage.
     */
    bool readProcessMemoryUsage();

    /**
     * @brief Updates the history of memory usage percentages.
     *
     * Maintains a fixed-size history to calculate average usage.
     */
    void updateUsageHistory();

    /**
     * @brief The loop function run by the update thread to periodically refresh memory metrics.
     */
    void updateLoop();

    unsigned long mem_total_;      ///< Total memory in KB.
    unsigned long mem_available_;  ///< Available memory in KB.
    unsigned long mem_free_;       ///< Free memory in KB.
    unsigned long mem_cached_;     ///< Cached memory in KB.
    unsigned long mem_buffers_;    ///< Buffered memory in KB.

    std::vector<float> usage_history_; ///< History of memory usage percentages.
    size_t max_history_size_;          ///< Maximum size of the usage history.

    std::unordered_map<std::string, unsigned long> process_memory_usage_; ///< Mapping of process names to their VmRSS in KB.

    std::chrono::steady_clock::time_point last_process_update_; ///< Timestamp of the last process memory usage update.
    std::chrono::seconds process_update_interval_;             ///< Interval between process memory usage updates.

    std::mutex data_mutex_; ///< Mutex to protect shared data.
    std::thread update_thread_; ///< Thread for updating memory metrics.
    std::atomic<bool> running_; ///< Flag to control the update thread.

    static constexpr const char* PROC_MEMINFO = "/proc/meminfo"; ///< Path to /proc/meminfo.
    static constexpr const char* PROC_DIR = "/proc";             ///< Path to /proc directory.
};

#endif // __linux__
#endif // MEMORY_LINUX_H

