// File: src/core/network/network_linux.cpp

#ifdef __linux__

#include "network_linux.h"
#include "utils/logger.h"
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <algorithm>

/**
 * @brief Retrieves the MAC address of the specified network interface.
 *
 * @param dev The name of the network interface (e.g., "eth0").
 * @param mac A string to store the retrieved MAC address.
 * @return true if the MAC address was successfully retrieved, false otherwise.
 */
bool LinuxNetwork::getInterfaceMAC(const std::string& dev, std::string& mac) {
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        Logger::log("Socket creation failed while retrieving MAC address.");
        return false;
    }

    std::strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
        Logger::log("ioctl failed while retrieving MAC address for interface: " + dev);
        close(fd);
        return false;
    }
    close(fd);

    unsigned char* mac_ptr = reinterpret_cast<unsigned char*>(ifr.ifr_hwaddr.sa_data);
    char mac_buf[18];
    std::snprintf(mac_buf, sizeof(mac_buf),
                  "%02x:%02x:%02x:%02x:%02x:%02x",
                  mac_ptr[0], mac_ptr[1], mac_ptr[2],
                  mac_ptr[3], mac_ptr[4], mac_ptr[5]);
    mac = std::string(mac_buf);
    return true;
}

/**
 * @brief Constructs a new LinuxNetwork object.
 *
 * Initializes network monitoring by setting up packet capture on the primary network interface.
 */
LinuxNetwork::LinuxNetwork()
    : handle(nullptr),
      bytes_received(0),
      bytes_sent(0),
      upload_rate(0.0f),
      download_rate(0.0f),
      highest_upload_rate(0.0f),
      highest_download_rate(0.0f),
      running(true) {
    // Determine the primary network interface (e.g., eth0, wlan0)
    char buffer[128];
    std::string primary_interface = "eth0"; // Default interface
    FILE* fp = popen("ip route | grep default | awk '{print $5}'", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            primary_interface = std::string(buffer);
            // Remove any trailing newline characters
            primary_interface.erase(std::remove(primary_interface.begin(), primary_interface.end(), '\n'), primary_interface.end());
        }
        pclose(fp);
    }
    interface_name = primary_interface;
    Logger::log("Primary network interface detected: " + interface_name);

    // Get MAC address of the interface
    if (!getInterfaceMAC(interface_name, mac_address)) {
        Logger::log("Failed to retrieve MAC address for interface: " + interface_name);
    } else {
        Logger::log("MAC Address of " + interface_name + ": " + mac_address);
    }

    startCapture();
    // Start the rate update thread
    update_thread = std::thread(&LinuxNetwork::updateRatesPeriodically, this);
}

/**
 * @brief Destructs the LinuxNetwork object.
 *
 * Stops packet capture and joins the update thread.
 */
LinuxNetwork::~LinuxNetwork() {
    running = false;
    stopCapture();
    if (update_thread.joinable()) {
        update_thread.join();
    }
}

/**
 * @brief Starts packet capturing on the specified network interface.
 */
void LinuxNetwork::startCapture() {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *alldevs, *dev;

    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        Logger::log("Error in pcap_findalldevs: " + std::string(errbuf));
        return;
    }

    // Find the specified interface
    dev = alldevs;
    while (dev != nullptr) {
        if (interface_name == dev->name) {
            break;
        }
        dev = dev->next;
    }

    if (dev == nullptr) {
        Logger::log("Interface " + interface_name + " not found. Make sure it exists and you have the necessary permissions.");
        pcap_freealldevs(alldevs);
        return;
    }

    handle = pcap_open_live(dev->name, BUFSIZ, 1, 1000, errbuf);
    if (handle == nullptr) {
        Logger::log("Error opening device " + std::string(dev->name) + ": " + std::string(errbuf));
        pcap_freealldevs(alldevs);
        return;
    }

    pcap_freealldevs(alldevs);

    // Start the packet capture thread
    capture_thread = std::thread([this]() {
        if (pcap_loop(handle, 0, packetHandler, reinterpret_cast<u_char*>(this)) < 0) {
            Logger::log("pcap_loop exited with error: " + std::string(pcap_geterr(handle)));
        }
    });

    last_update_time = std::chrono::steady_clock::now();
}

/**
 * @brief Stops packet capturing and closes the pcap handle.
 */
void LinuxNetwork::stopCapture() {
    if (handle) {
        pcap_breakloop(handle);
        pcap_close(handle);
        handle = nullptr;
    }
    if (capture_thread.joinable()) {
        capture_thread.join();
    }
}

/**
 * @brief Static callback function for pcap_loop to handle packets.
 *
 * Determines whether a packet is incoming or outgoing based on MAC address.
 * @param userData Pointer to user data (this instance).
 * @param pkthdr Packet header provided by libpcap.
 * @param packet The actual packet data.
 */
void LinuxNetwork::packetHandler(u_char* userData, const struct pcap_pkthdr* pkthdr, const u_char* packet) {
    LinuxNetwork* network = reinterpret_cast<LinuxNetwork*>(userData);

    // Ethernet header is 14 bytes
    const struct ether_header* eth_header = reinterpret_cast<const struct ether_header*>(packet);
    std::string src_mac = "";
    char buffer[18]; // Buffer to store MAC address string

    std::snprintf(buffer, sizeof(buffer),
                  "%02x:%02x:%02x:%02x:%02x:%02x",
                  eth_header->ether_shost[0],
                  eth_header->ether_shost[1],
                  eth_header->ether_shost[2],
                  eth_header->ether_shost[3],
                  eth_header->ether_shost[4],
                  eth_header->ether_shost[5]);
    src_mac = std::string(buffer);

    // Determine if the packet is outgoing or incoming
    if (src_mac == network->mac_address) {
        network->bytes_sent += pkthdr->len;
    } else {
        network->bytes_received += pkthdr->len;
    }
}

/**
 * @brief Periodically updates upload and download rates based on captured data.
 */
void LinuxNetwork::updateRatesPeriodically() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::lock_guard<std::mutex> lock(rate_mutex);
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time).count();

        if (duration > 0) {
            float duration_sec = duration / 1000.0f;
            // Convert bytes to megabytes and calculate rate in MB/s
            float current_upload_rate = (bytes_sent.load() / (1024.0f * 1024.0f)) / duration_sec;    // MB/s
            float current_download_rate = (bytes_received.load() / (1024.0f * 1024.0f)) / duration_sec; // MB/s

            upload_rate = current_upload_rate;
            download_rate = current_download_rate;

            if (current_upload_rate > highest_upload_rate) {
                highest_upload_rate = current_upload_rate;
            }
            if (current_download_rate > highest_download_rate) {
                highest_download_rate = current_download_rate;
            }

            // Reset the counters
            bytes_sent = 0;
            bytes_received = 0;
            last_update_time = now;
        }
    }
}

/**
 * @brief Retrieves the total network bandwidth.
 *
 * @return float Total bandwidth in Mbps (placeholder value).
 */
float LinuxNetwork::getTotalBandwidth() {
    // This would typically be determined by the network interface capabilities
    // For simplicity, we'll return a placeholder value
    return 1000.0f; // Assume 1 Gbps
}

/**
 * @brief Retrieves the current upload rate in MB/s.
 *
 * @return float Upload rate in MB/s.
 */
float LinuxNetwork::getUploadRate() {
    std::lock_guard<std::mutex> lock(rate_mutex);
    return upload_rate;
}

/**
 * @brief Retrieves the current download rate in MB/s.
 *
 * @return float Download rate in MB/s.
 */
float LinuxNetwork::getDownloadRate() {
    std::lock_guard<std::mutex> lock(rate_mutex);
    return download_rate;
}

/**
 * @brief Retrieves the total used bandwidth since monitoring started.
 *
 * @return float Total used bandwidth in MB.
 */
float LinuxNetwork::getTotalUsedBandwidth() {
    std::lock_guard<std::mutex> lock(rate_mutex);
    return upload_rate + download_rate;
}

/**
 * @brief Retrieves the highest upload rate recorded.
 *
 * @return float Highest upload rate in MB/s.
 */
float LinuxNetwork::getHighestUploadRate() {
    std::lock_guard<std::mutex> lock(rate_mutex);
    return highest_upload_rate;
}

/**
 * @brief Retrieves the highest download rate recorded.
 *
 * @return float Highest download rate in MB/s.
 */
float LinuxNetwork::getHighestDownloadRate() {
    std::lock_guard<std::mutex> lock(rate_mutex);
    return highest_download_rate;
}

/**
 * @brief Retrieves the name of the process consuming the most network bandwidth.
 *
 * @return std::string Name of the top bandwidth-consuming process.
 */
std::string LinuxNetwork::getTopBandwidthProcess() {
    // This would require additional system-specific implementations
    // For now, we'll return a placeholder
    return "Not Implemented";
}

#endif // __linux__

