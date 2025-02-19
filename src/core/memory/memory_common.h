// File: src/core/memory/memory_common.h

#ifndef MEMORY_COMMON_H
#define MEMORY_COMMON_H

#include <string>

/**
 * @class Memory
 * @brief Abstract base class for Memory monitoring.
 *
 * Provides an interface for retrieving memory usage statistics.
 */
class Memory {
public:
    /**
     * @brief Virtual destructor for the Memory interface.
     */
    virtual ~Memory() = default;

    /**
     * @brief Retrieves the total memory usage percentage.
     * @return Total memory usage as a percentage (0.0 - 100.0).
     */
    virtual float getTotalUsage() = 0;

    /**
     * @brief Retrieves the remaining RAM in megabytes.
     * @return Remaining RAM in MB.
     */
    virtual float getRemainingRAM() = 0;

    /**
     * @brief Retrieves the average memory usage percentage over time.
     * @return Average memory usage as a percentage.
     */
    virtual float getAverageUsage() = 0;

    /**
     * @brief Retrieves the name of the process consuming the most memory.
     * @return Name of the top memory-consuming process.
     */
    virtual std::string getMostUsingProcess() = 0;
};

/**
 * @brief Factory function to create a Memory instance.
 *
 * @return Memory* Pointer to a newly created Memory instance.
 */
Memory* createMemory();

#endif // MEMORY_COMMON_H


