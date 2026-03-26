/**
 * @file process_common.h
 * @brief Abstract base class for process management.
 *
 * Platform implementations derive from ProcessManager and override
 * update() / snapshot() / killProcess() / setProcessPriority().
 * The collector thread calls update() periodically; any reader calls
 * snapshot() to obtain a thread-safe copy of the latest ProcessSnapshot.
 */

#pragma once

#include "../metrics.h"
#include <memory>

/**
 * @class ProcessManager
 * @brief Interface for process monitoring and management.
 */
class ProcessManager {
public:
    virtual ~ProcessManager() = default;

    /**
     * @brief Collect fresh process data from the operating system.
     *
     * Called by the collector thread. Implementations should enumerate
     * all processes, compute CPU/memory usage deltas, and store the
     * result under a lock for thread-safe access via snapshot().
     */
    virtual void update() = 0;

    /**
     * @brief Return a thread-safe copy of the most recent snapshot.
     */
    virtual ProcessSnapshot snapshot() const = 0;

    /**
     * @brief Attempt to terminate a process by PID.
     * @param pid The process ID to kill.
     * @return true if the process was successfully terminated, false otherwise.
     */
    virtual bool killProcess(int pid) = 0;

    /**
     * @brief Attempt to change a process's scheduling priority.
     * @param pid The process ID.
     * @param priority Platform-interpreted priority value.
     * @return true if the priority was successfully changed, false otherwise.
     */
    virtual bool setProcessPriority(int pid, int priority) = 0;
};

/**
 * @brief Factory: returns a platform-specific ProcessManager instance.
 */
std::unique_ptr<ProcessManager> createProcessManager();
