#ifndef NETWORK_WINDOWS_H
#define NETWORK_WINDOWS_H

#ifdef _WIN32

#include <pdh.h>
#include <iphlpapi.h>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

#include "network_common.h"

/**
 * @class WindowsNetwork
 * @brief Windows-specific implementation of the Network monitoring interface.
 */
class WindowsNetwork : public Network {
public:
    WindowsNetwork();
    ~WindowsNetwork() override;

    float getTotalBandwidth() override;
    float getUploadRate() override;
    float getDownloadRate() override;
    float getTotalUsedBandwidth() override;
    float getHighestUploadRate() override;
    float getHighestDownloadRate() override;
    std::string getTopBandwidthProcess() override;

private:
    void updateLoop();
    void updateNetworkStats();

    std::atomic<bool> running_;
    std::thread update_thread_;
    std::mutex data_mutex_;

    float currentUploadRate_;
    float currentDownloadRate_;
    float highestUploadRate_;
    float highestDownloadRate_;

    PDH_HQUERY hQuery_;
    PDH_HCOUNTER hCounterSend_;
    PDH_HCOUNTER hCounterReceive_;
};

#endif // _WIN32

#endif // NETWORK_WINDOWS_H
