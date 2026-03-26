/**
 * @file gpu_factory.cpp
 * @brief Creates the platform-appropriate GPU monitor instance.
 */

#include "gpu_common.h"

#ifdef _WIN32
#include "gpu_windows.h"
#elif defined(__linux__)
#include "gpu_linux.h"
#else
#error "Unsupported platform"
#endif

/**
 * @brief Create a GPU monitor for the current platform.
 * @return Owning pointer to a WindowsGPU or LinuxGPU instance.
 */
std::unique_ptr<GPU> createGPU() {
#ifdef _WIN32
    return std::make_unique<WindowsGPU>();
#elif defined(__linux__)
    return std::make_unique<LinuxGPU>();
#else
    return nullptr;
#endif
}
