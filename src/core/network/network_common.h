// File: src/core/network/network_common.h

#ifndef NETWORK_COMMON_H
#define NETWORK_COMMON_H

#include <string>

/**
 * @class Network
 * @brief Abstract base class for Network monitoring.
 *
 * Provides an interface for retrieving Network usage statistics such as
 * upload/download rates, total bandwidth, and processes consuming bandwidth.
 */
class Network {
public:
    /**
     * @brief Virtual destructor for the Network interface.
     */
    virtual ~Network() = default;

    /**
     * @brief Retrieves the total network bandwidth in Mbps.
     * @return Total bandwidth in Mbps.
     */
    virtual float getTotalBandwidth() = 0;

    /**
     * @brief Retrieves the current upload rate in MB/s.
     * @return Upload rate in MB/s.
     */
    virtual float getUploadRate() = 0;

    /**
     * @brief Retrieves the current download rate in MB/s.
     * @return Download rate in MB/s.
     */
    virtual float getDownloadRate() = 0;

    /**
     * @brief Retrieves the total used bandwidth since monitoring started in MB.
     * @return Total used bandwidth in MB.
     */
    virtual float getTotalUsedBandwidth() = 0;

    /**
     * @brief Retrieves the highest upload rate recorded in MB/s.
     * @return Highest upload rate in MB/s.
     */
    virtual float getHighestUploadRate() = 0;

    /**
     * @brief Retrieves the highest download rate recorded in MB/s.
     * @return Highest download rate in MB/s.
     */
    virtual float getHighestDownloadRate() = 0;

    /**
     * @brief Retrieves the name of the process consuming the most network bandwidth.
     * @return Name of the top bandwidth-consuming process.
     */
    virtual std::string getTopBandwidthProcess() = 0;
};

/**
 * @brief Factory function to create a Network instance.
 *
 * @return Network* Pointer to a newly created Network instance.
 */
Network* createNetwork();

#endif // NETWORK_COMMON_H

