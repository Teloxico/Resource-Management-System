// File: src/core/network/network_factory.cpp

#include "network_common.h"

#ifdef _WIN32
#include "network_windows.h"
#elif defined(__linux__)
#include "network_linux.h"
#else
#error "Unsupported platform"
#endif

/**
 * @brief Factory function to create a Network instance based on the platform.
 *
 * @return Network* Pointer to a newly created Network instance for Windows or Linux.
 */
Network* createNetwork() {
#ifdef _WIN32
    return new WindowsNetwork();
#elif defined(__linux__)
    return new LinuxNetwork();
#else
    return nullptr;
#endif
}

