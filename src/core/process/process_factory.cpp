/**
 * @file process_factory.cpp
 * @brief Factory function for creating a platform-specific ProcessManager.
 */

#include "process_common.h"

#ifdef _WIN32
#include "process_windows.h"
#elif defined(__linux__)
#include "process_linux.h"
#else
#error "Unsupported platform"
#endif

/**
 * @brief Factory function to create a ProcessManager instance based on the platform.
 * @return std::unique_ptr<ProcessManager> owning the concrete implementation.
 */
std::unique_ptr<ProcessManager> createProcessManager() {
#ifdef _WIN32
    return std::make_unique<WindowsProcessManager>();
#elif defined(__linux__)
    return std::make_unique<LinuxProcessManager>();
#else
    return nullptr;
#endif
}
