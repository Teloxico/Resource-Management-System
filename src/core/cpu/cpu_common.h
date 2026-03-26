/**
 * @file cpu_common.h
 * @brief Abstract base class and factory declaration for CPU monitoring.
 */

#pragma once

#include "../metrics.h"
#include <memory>

/**
 * @brief Abstract interface for platform-specific CPU monitors.
 */
class CPU {
public:
    virtual ~CPU() = default;

    /**
     * @brief Collect fresh CPU metrics from the OS.
     */
    virtual void update() = 0;

    /**
     * @brief Get a thread-safe copy of the latest CPU metrics.
     * @return Most recent CpuSnapshot.
     */
    virtual CpuSnapshot snapshot() const = 0;
};

/**
 * @brief Create a platform-specific CPU monitor instance.
 * @return Unique pointer to a CPU implementation.
 */
std::unique_ptr<CPU> createCPU();
