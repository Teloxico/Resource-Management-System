/**
 * @file cpu_factory.cpp
 * @brief Factory function returning a platform-specific CPU monitor.
 */

#include "cpu_common.h"

#ifdef _WIN32
#include "cpu_windows.h"
#elif defined(__linux__)
#include "cpu_linux.h"
#else
#error "Unsupported platform"
#endif

std::unique_ptr<CPU> createCPU() {
#ifdef _WIN32
    return std::make_unique<WindowsCPU>();
#else
    return std::make_unique<LinuxCPU>();
#endif
}
