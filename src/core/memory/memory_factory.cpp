/**
 * @file memory_factory.cpp
 * @brief Creates the platform-appropriate Memory implementation.
 */

#include "memory_common.h"

#ifdef _WIN32
#include "memory_windows.h"
#elif defined(__linux__)
#include "memory_linux.h"
#else
#error "Unsupported platform"
#endif

/**
 * @brief Instantiate the correct Memory subclass for the current OS.
 * @return Owning pointer to a WindowsMemory or LinuxMemory instance.
 */
std::unique_ptr<Memory> createMemory() {
#ifdef _WIN32
    return std::make_unique<WindowsMemory>();
#else
    return std::make_unique<LinuxMemory>();
#endif
}
