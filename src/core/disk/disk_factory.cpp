/**
 * @file disk_factory.cpp
 * @brief Creates the platform-appropriate Disk implementation.
 */

#include "disk_common.h"

#ifdef _WIN32
#include "disk_windows.h"
#elif defined(__linux__)
#include "disk_linux.h"
#else
#error "Unsupported platform"
#endif

/**
 * @brief Create a Disk instance for the current platform.
 * @return Owning pointer to a WindowsDisk or LinuxDisk.
 */
std::unique_ptr<Disk> createDisk() {
#ifdef _WIN32
    return std::make_unique<WindowsDisk>();
#elif defined(__linux__)
    return std::make_unique<LinuxDisk>();
#else
    return nullptr;
#endif
}
