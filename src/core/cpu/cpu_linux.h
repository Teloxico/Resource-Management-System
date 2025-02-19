// File: src/core/cpu/cpu_linux.h

#ifndef CPU_LINUX_H
#define CPU_LINUX_H

#ifdef __linux__

#include "cpu_common.h"
#include <vector>

/**
 * @class LinuxCPU
 * @brief Linux-specific implementation of the CPU monitoring interface.
 *
 * Provides methods to retrieve CPU usage statistics on Linux systems.
 */
class LinuxCPU : public CPU {
public:
    /**
     * @brief Constructor for LinuxCPU.
     *
     * Initializes CPU usage tracking variables.
     */
    LinuxCPU();

    /**
     * @brief Destructor for LinuxCPU.
     */
    ~LinuxCPU() override;

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
     * @brief Retrieves the number of threads currently in use.
     * @return Number of used threads.
     */
    int getUsedThreads() override;

    /**
     * @brief Retrieves the total number of threads available.
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
     * @brief Reads CPU statistics from /proc/stat.
     * @param stats Vector to store CPU statistics.
     * @return True if successful, False otherwise.
     */
    bool readCPUStat(std::vector<unsigned long long>& stats);

    unsigned long long prev_total_;   ///< Previous total CPU time.
    unsigned long long prev_idle_;    ///< Previous idle CPU time.
    std::vector<float> usage_history_;///< History of CPU usage percentages.
    size_t max_history_size_;         ///< Maximum size of usage history.
};

#endif // __linux__

#endif // CPU_LINUX_H

