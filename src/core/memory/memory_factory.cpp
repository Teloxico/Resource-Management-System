// File: src/core/memory/memory_factory.cpp

#include "memory_common.h"

#ifdef _WIN32
#include "memory_windows.h"
#elif defined(__linux__)
#include "memory_linux.h"
#else
#error "Unsupported platform"
#endif

/**
 * @brief Factory function to create a Memory instance based on the platform.
 *
 * @return Memory* Pointer to a newly created Memory instance for Windows or Linux.
 */
Memory* createMemory() {
#ifdef _WIN32
    return new WindowsMemory();
#elif defined(__linux__)
    return new LinuxMemory();
#else
    return nullptr;
#endif
}

