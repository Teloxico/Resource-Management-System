// File: src/core/memory/memory_windows.h

#ifndef MEMORY_WINDOWS_H
#define MEMORY_WINDOWS_H

#ifdef _WIN32

#include "memory_common.h"
#include <windows.h>
#include <vector>

/**
 * @class WindowsMemory
 * @brief Windows-specific implementation for Memory monitoring.
 *
 * Utilizes Windows API to monitor memory usage.
 */
class WindowsMemory : public Memory {
public:
    /**
     * @brief Constructs a new WindowsMemory object.
     */
    WindowsMemory();

    /**
     * @brief Destructs the WindowsMemory object.
     */
    ~WindowsMemory() override = default;

    /**
     * @brief Retrieves the total memory usage percentage.
     * @return float Total memory usage as a percentage.
     */
    float getTotalUsage() override;

    /**
     * @brief Retrieves the remaining RAM in megabytes.
     * @return float Remaining RAM in MB.
     */
    float getRemainingRAM() override;

    /**
     * @brief Retrieves the average memory usage percentage over time.
     * @return float Average memory usage as a percentage.
     */
    float getAverageUsage() override;

    /**
     * @brief Retrieves the name of the process consuming the most memory.
     * @return std::string Name of the top memory-consuming process.
     */
    std::string getMostUsingProcess() override;

private:
    std::vector<float> usage_history_; ///< History of memory usage percentages.
    const size_t max_history_size_ = 100; ///< Maximum size of the usage history.
};

#endif // _WIN32

#endif // MEMORY_WINDOWS_H

