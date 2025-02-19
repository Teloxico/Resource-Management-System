// File: src/core/cpu/cpu_factory.cpp

#include "cpu_common.h"

#ifdef _WIN32
#include "cpu_windows.h"
#elif defined(__linux__)
#include "cpu_linux.h"
#else
#error "Unsupported platform"
#endif

/**
 * @brief Factory function to create a CPU instance based on the platform.
 *
 * @return CPU* Pointer to a newly created CPU instance for Windows or Linux.
 */
CPU* createCPU() {
#ifdef _WIN32
    return new WindowsCPU();
#elif defined(__linux__)
    return new LinuxCPU();
#else
    return nullptr;
#endif
}

