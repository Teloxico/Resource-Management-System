/**
 * @file gpu_common.h
 * @brief Abstract base class and factory declaration for GPU monitoring.
 */

#pragma once

#include "../metrics.h"
#include <memory>

/**
 * @brief Abstract interface for platform-specific GPU monitors.
 */
class GPU {
public:
    virtual ~GPU() = default;

    /**
     * @brief Refresh GPU metrics from the OS or driver.
     */
    virtual void update() = 0;

    /**
     * @brief Get a thread-safe copy of the latest GPU metrics.
     * @return Most recent GpuSnapshot.
     */
    virtual GpuSnapshot snapshot() const = 0;
};

/**
 * @brief Create a platform-specific GPU monitor instance.
 * @return Owning pointer to the concrete GPU implementation.
 */
std::unique_ptr<GPU> createGPU();
