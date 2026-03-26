/**
 * @file network_common.h
 * @brief Abstract base class and factory for platform-specific network monitoring.
 */

#pragma once

#include "../metrics.h"
#include <memory>

/**
 * @brief Abstract interface for collecting network metrics.
 */
class Network {
public:
    virtual ~Network() = default;

    /**
     * @brief Collect fresh network metrics from the OS.
     */
    virtual void update() = 0;

    /**
     * @brief Return a thread-safe copy of the latest metrics.
     * @return NetworkSnapshot from the most recent update() call.
     */
    virtual NetworkSnapshot snapshot() const = 0;
};

/**
 * @brief Create a platform-specific Network instance.
 * @return Owning pointer to the concrete implementation.
 */
std::unique_ptr<Network> createNetwork();
