// File: src/core/network/network_linux.h

#ifndef NETWORK_LINUX_H
#define NETWORK_LINUX_H

#ifdef __linux__

#include "network_common.h"
#include <string>
#include <map>
#include <chrono>
#include <pcap.h>
#include <atomic>
#include <thread>
#include <mutex>

/**
 * @class LinuxNetwork
 * @brief Linux-specific implementation for Network monitoring.
 *
 * Utilizes libpcap for capturing network packets and calculates upload/download rates.
 */
class LinuxNetwork : public Network {
public:
    /**
     * @brief Constructs a new LinuxNetwork object.
     *
     * Initializes network monitoring using libpcap and starts capture threads.
     */
    LinuxNetwork();

    /**
     * @brief Destructs the LinuxNetwork object.
     *
     * Stops packet capture and joins threads.
     */
    ~LinuxNetwork() override;

    /**
     * @brief Retrieves the total network bandwidth (placeholder value).
     * @return Total bandwidth in Mbps.
     */
    float getTotalBandwidth() override;

    /**
     * @brief Retrieves the current upload rate in MB/s.
     * @return Upload rate in MB/s.
     */
    float getUploadRate() override;

    /**
     * @brief Retrieves the current download rate in MB/s.
     * @return Download rate in MB/s.
     */
    float getDownloadRate() override;

    /**
     * @brief Retrieves the total used bandwidth since monitoring started in MB.
     * @return Total used bandwidth in MB.
     */
    float getTotalUsedBandwidth() override;

    /**
     * @brief Retrieves the highest upload rate recorded in MB/s.
     * @return Highest upload rate in MB/s.
     */
    float getHighestUploadRate() override;

    /**
     * @brief Retrieves the highest download rate recorded in MB/s.
     * @return Highest download rate in MB/s.
     */
    float getHighestDownloadRate() override;

    /**
     * @brief Retrieves the name of the process consuming the most network bandwidth.
     * @return Name of the top bandwidth-consuming process.
     */
    std::string getTopBandwidthProcess() override;

private:
    pcap_t* handle;                        ///< libpcap handle for packet capture.
    std::atomic<uint64_t> bytes_received;  ///< Total bytes received.
    std::atomic<uint64_t> bytes_sent;      ///< Total bytes sent.
    std::chrono::steady_clock::time_point last_update_time; ///< Timestamp of the last rate update.
    float upload_rate;                     ///< Current upload rate in MB/s.
    float download_rate;                   ///< Current download rate in MB/s.
    float highest_upload_rate;             ///< Highest upload rate recorded.
    float highest_download_rate;           ///< Highest download rate recorded.
    std::thread capture_thread;            ///< Thread for packet capturing.
    std::thread update_thread;             ///< Thread for updating rates periodically.
    std::atomic<bool> running;             ///< Flag to control the capture and update threads.
    std::mutex rate_mutex;                 ///< Mutex to protect rate variables.
    std::string interface_name;            ///< Name of the network interface being monitored.
    std::string mac_address;               ///< MAC address of the network interface.

    /**
     * @brief Starts packet capturing on the specified network interface.
     */
    void startCapture();

    /**
     * @brief Stops packet capturing and closes the pcap handle.
     */
    void stopCapture();

    /**
     * @brief Static callback function for pcap_loop to handle packets.
     * @param userData Pointer to user data (this instance).
     * @param pkthdr Packet header provided by libpcap.
     * @param packet The actual packet data.
     */
    static void packetHandler(u_char* userData, const struct pcap_pkthdr* pkthdr, const u_char* packet);

    /**
     * @brief Periodically updates upload and download rates based on captured data.
     */
    void updateRatesPeriodically();

    /**
     * @brief Retrieves the MAC address of the specified network interface.
     * @param dev The name of the network interface.
     * @param mac String to store the retrieved MAC address.
     * @return True if successful, false otherwise.
     */
    bool getInterfaceMAC(const std::string& dev, std::string& mac);
};

#endif // __linux__
#endif // NETWORK_LINUX_H


