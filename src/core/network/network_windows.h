/**
 * @file network_windows.h
 * @brief Windows implementation of the Network monitoring interface.
 */

#pragma once

#ifdef _WIN32

#include "network_common.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <chrono>

/**
 * @brief Windows network monitor using Win32 IP Helper and PSAPI.
 */
class WindowsNetwork : public Network {
public:
    WindowsNetwork();
    ~WindowsNetwork() override;

    /**
     * @brief Gather per-interface bandwidth, addresses, and TCP/UDP connections.
     */
    void update() override;

    /**
     * @brief Return a thread-safe copy of the latest snapshot.
     * @return NetworkSnapshot with current network data.
     */
    NetworkSnapshot snapshot() const override;

private:
    /// Per-interface byte counters from the previous sample.
    struct IfPrev {
        uint64_t inOctets  = 0;
        uint64_t outOctets = 0;
    };

    mutable std::mutex mtx_;              ///< Guards snap_ for thread-safe reads.
    NetworkSnapshot snap_;                ///< Most recent snapshot from update().
    float highestUpload_   = 0.0f;        ///< Lifetime peak upload rate (bytes/s).
    float highestDownload_ = 0.0f;        ///< Lifetime peak download rate (bytes/s).
    std::unordered_map<uint32_t, IfPrev> prevCounters_; ///< Previous counters by interface index.
    std::chrono::steady_clock::time_point prevTime_;    ///< Timestamp of previous update().
    bool hasPrevSample_ = false;          ///< True after at least one update() completes.
    std::unordered_map<int, std::string> processNameCache_; ///< PID-to-name lookup cache.

    /**
     * @brief Resolve a PID to its process name, using the cache.
     * @param pid Process identifier.
     * @return Process name or "Unknown"/"System".
     */
    std::string resolveProcessName(int pid);

    /**
     * @brief Convert a MIB_TCP_STATE value to a human-readable string.
     * @param state Windows TCP state constant.
     * @return State name such as "ESTABLISHED" or "LISTEN".
     */
    static std::string tcpStateToString(int state);
};

#endif
