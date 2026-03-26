/**
 * @file memory_common.h
 * @brief Abstract base class and factory declaration for memory monitoring.
 */

#pragma once

#include "../metrics.h"
#include <memory>

/**
 * @brief Platform-independent interface for memory monitoring.
 */
class Memory {
public:
    virtual ~Memory() = default;

    /**
     * @brief Collect current memory metrics from the OS.
     */
    virtual void update() = 0;

    /**
     * @brief Get a thread-safe copy of the latest memory data.
     * @return Most recent MemorySnapshot.
     */
    virtual MemorySnapshot snapshot() const = 0;
};

/**
 * @brief Create a platform-specific Memory implementation.
 * @return Owning pointer to the concrete Memory subclass.
 */
std::unique_ptr<Memory> createMemory();
