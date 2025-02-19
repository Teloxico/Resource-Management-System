// File: src/core/cpu/cpu_windows.h

#ifndef CPU_WINDOWS_H
#define CPU_WINDOWS_H

#ifdef _WIN32

#include "cpu_common.h"
#include <windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <tlhelp32.h>
#include <Wbemidl.h>
#include <pdh.h>

#pragma comment(lib, "wbemuuid.lib")

/**
 * @class WindowsCPU
 * @brief Windows-specific implementation of the CPU monitoring interface.
 *
 * Monitors CPU usage statistics on Windows systems.
 */
class WindowsCPU : public CPU {
public:
    /**
     * @brief Constructor for WindowsCPU.
     *
     * Initializes variables and starts the CPU usage update thread.
     */
    WindowsCPU();

    /**
     * @brief Destructor for WindowsCPU.
     *
     * Stops the update thread.
     */
    ~WindowsCPU() override;

    /**
     * @brief Retrieves the total CPU usage percentage.
     * @return Total CPU usage as a percentage.
     */
    float getTotalUsage() override;

    /**
     * @brief Retrieves the CPU clock frequency in GHz.
     * @return CPU clock frequency in GHz.
     */
    float getClockFrequency() override;

    /**
     * @brief Retrieves the number of threads currently used by the current process.
     * @return Number of threads used by the current process.
     */
    int getUsedThreads() override;

    /**
     * @brief Retrieves the total number of threads in the system.
     * @return Total number of threads.
     */
    int getTotalThreads() override;

    /**
     * @brief Retrieves the highest CPU usage recorded.
     * @return Highest CPU usage as a percentage.
     */
    float getHighestUsage() override;

    /**
     * @brief Retrieves the average CPU usage over time.
     * @return Average CPU usage as a percentage.
     */
    float getAverageUsage() override;

private:
    /**
     * @brief Thread function to periodically update CPU usage.
     */
    void updateLoop();

    /**
     * @brief Calculates the CPU usage using GetSystemTimes.
     * @return CPU usage as a percentage.
     */
    float calculateCPUUsage();

    /**
     * @brief Helper function to convert FILETIME to ULONGLONG.
     * @param ft FILETIME structure.
     * @return ULONGLONG representation.
     */
    ULONGLONG fileTimeToInt64(const FILETIME &ft);

    std::vector<float> usage_history_;             ///< History of total CPU usage percentages.
    static constexpr size_t max_history_size_ = 100; ///< Maximum size of usage history.
    float highest_usage_;                          ///< Highest recorded CPU usage percentage.

    std::thread update_thread_;                    ///< Thread for updating CPU usage.
    std::atomic<bool> running_;                    ///< Flag to control the update thread.
    std::mutex data_mutex_;                        ///< Mutex to protect shared data.
    float current_usage_;                          ///< Current CPU usage percentage.

    // Previous times for CPU usage calculation
    FILETIME idleTime_;
    FILETIME kernelTime_;
    FILETIME userTime_;
    ULONGLONG prev_idle_time_;
    ULONGLONG prev_kernel_time_;
    ULONGLONG prev_user_time_;
};

#endif // _WIN32

#endif // CPU_WINDOWS_H
