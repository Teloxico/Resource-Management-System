/**
 * @file network_factory.cpp
 * @brief Factory that creates the platform-specific Network implementation.
 */

#include "network_common.h"

#ifdef _WIN32
#include "network_windows.h"
#elif defined(__linux__)
#include "network_linux.h"
#else
#error "Unsupported platform"
#endif

std::unique_ptr<Network> createNetwork() {
#ifdef _WIN32
    return std::make_unique<WindowsNetwork>();
#elif defined(__linux__)
    return std::make_unique<LinuxNetwork>();
#else
    return nullptr;
#endif
}
