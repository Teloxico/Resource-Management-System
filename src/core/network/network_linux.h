/**
 * @file network_linux.h
 * @brief Linux implementation of the Network monitoring interface.
 */

#pragma once

#ifdef __linux__

#include "network_common.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <chrono>

/**
 * @brief Linux network monitor using /proc and sysfs.
 */
class LinuxNetwork : public Network {
public:
    LinuxNetwork();
    ~LinuxNetwork() override;

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
    /// Per-interface byte and packet counters from the previous sample.
    struct IfPrev {
        uint64_t rxBytes   = 0;
        uint64_t txBytes   = 0;
        uint64_t rxPackets = 0;
        uint64_t txPackets = 0;
        uint64_t rxErrors  = 0;
        uint64_t txErrors  = 0;
        uint64_t rxDrops   = 0;
        uint64_t txDrops   = 0;
    };

    /// Maps socket inode numbers to owning PIDs.
    using InodePidMap = std::unordered_map<uint64_t, int>;

    mutable std::mutex mtx_;              ///< Guards snap_ for thread-safe reads.
    NetworkSnapshot snap_;                ///< Most recent snapshot from update().
    float highestUpload_   = 0.0f;        ///< Lifetime peak upload rate (bytes/s).
    float highestDownload_ = 0.0f;        ///< Lifetime peak download rate (bytes/s).
    std::unordered_map<std::string, IfPrev> prevCounters_; ///< Previous counters by interface name.
    std::chrono::steady_clock::time_point prevTime_;       ///< Timestamp of previous update().
    bool hasPrevSample_ = false;          ///< True after at least one update() completes.
    std::unordered_map<int, std::string> processNameCache_; ///< PID-to-name lookup cache.
    InodePidMap inodePidMap_;             ///< Cached inode-to-PID mapping.
    std::chrono::steady_clock::time_point lastInodeScan_;  ///< When inodePidMap_ was last refreshed.

    /**
     * @brief Parse /proc/net/dev and populate interface info with counters and rates.
     * @param ifaces Output vector to append interface data to.
     * @param dtSec Elapsed seconds since last sample, for rate calculation.
     */
    void parseNetDev(std::vector<NetworkInterfaceInfo>& ifaces, double dtSec);

    /**
     * @brief Fill IP and MAC addresses on interfaces via getifaddrs().
     * @param ifaces Interfaces to enrich with address data.
     */
    void fillAddresses(std::vector<NetworkInterfaceInfo>& ifaces);

    /**
     * @brief Read link speed from sysfs for an interface.
     * @param iface Interface name (e.g. "eth0").
     * @return Link speed in Mbps, or 0 on failure.
     */
    static float readLinkSpeed(const std::string& iface);

    /**
     * @brief Check if an interface is up via sysfs operstate.
     * @param iface Interface name.
     * @return True if operstate is "up".
     */
    static bool readOperState(const std::string& iface);

    /**
     * @brief Parse /proc/net/tcp for IPv4 TCP connections.
     * @return Vector of parsed TCP connections.
     */
    std::vector<TcpConnection> parseTcpConnections();

    /**
     * @brief Parse /proc/net/tcp6 for IPv6 TCP connections.
     * @return Vector of parsed TCP connections.
     */
    std::vector<TcpConnection> parseTcp6Connections();

    /**
     * @brief Parse a /proc/net/udp or udp6 file for UDP endpoints.
     * @param path Path to the proc file (e.g. "/proc/net/udp").
     * @return Vector of parsed UDP connections.
     */
    std::vector<TcpConnection> parseUdpConnections(const std::string& path);

    /**
     * @brief Rebuild the inode-to-PID map by scanning /proc/[pid]/fd/.
     */
    void refreshInodePidMap();

    /**
     * @brief Resolve a PID to its process name, using the cache.
     * @param pid Process identifier.
     * @return Process name or "Unknown"/"N/A".
     */
    std::string resolveProcessName(int pid);

    /**
     * @brief Convert a hex TCP state code to a human-readable string.
     * @param state Linux TCP state value from /proc/net/tcp.
     * @return State name such as "ESTABLISHED" or "LISTEN".
     */
    static std::string tcpStateToString(int state);
};

#endif
