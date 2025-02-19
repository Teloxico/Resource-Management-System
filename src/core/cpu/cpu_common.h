// File: src/core/cpu/cpu_common.h

#ifndef CPU_COMMON_H
#define CPU_COMMON_H

#include <string>

/**
 * @class CPU
 * @brief Interface for CPU monitoring functionalities.
 *
 * Provides methods to retrieve CPU usage, clock frequency, thread count, etc.
 */
class CPU {
public:
    /**
     * @brief Virtual destructor for the CPU interface.
     */
    virtual ~CPU() = default;

    /**
     * @brief Retrieves the total CPU usage percentage.
     * @return Total CPU usage as a percentage (0.0 - 100.0).
     */
    virtual float getTotalUsage() = 0;

    /**
     * @brief Retrieves the CPU clock frequency in GHz.
     * @return CPU clock frequency in GHz.
     */
    virtual float getClockFrequency() = 0;

    /**
     * @brief Retrieves the number of threads currently in use.
     * @return Number of used threads.
     */
    virtual int getUsedThreads() = 0;

    /**
     * @brief Retrieves the total number of threads available.
     * @return Total number of threads.
     */
    virtual int getTotalThreads() = 0;

    /**
     * @brief Retrieves the highest CPU usage recorded.
     * @return Highest CPU usage as a percentage.
     */
    virtual float getHighestUsage() = 0;

    /**
     * @brief Retrieves the average CPU usage over time.
     * @return Average CPU usage as a percentage.
     */
    virtual float getAverageUsage() = 0;
};

/**
 * @brief Factory function to create a CPU instance.
 *
 * @return CPU* Pointer to a newly created CPU instance.
 */
CPU* createCPU();

#endif // CPU_COMMON_H

