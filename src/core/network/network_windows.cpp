#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // Targeting Windows 7 or later
#endif

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <winsock2.h>   // Must include before windows.h
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <vector>

#include "network_windows.h"
#include "utils/logger.h"

#pragma comment(lib, "Ws2_32.lib")  // Link with Ws2_32.lib
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

WindowsNetwork::WindowsNetwork()
    : running_(true),
      currentUploadRate_(0.0f),
      currentDownloadRate_(0.0f),
      highestUploadRate_(0.0f),
      highestDownloadRate_(0.0f),
      hQuery_(nullptr),
      hCounterSend_(nullptr),
      hCounterReceive_(nullptr)
{
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Logger::log("Failed to initialize Winsock.");
        return;
    }

    // Initialize PDH Query
    if (PdhOpenQuery(NULL, NULL, &hQuery_) != ERROR_SUCCESS) {
        Logger::log("Failed to open PDH query for network monitoring.");
        hQuery_ = nullptr;
        return;
    }

    // Use wildcard (*) to include all network adapters
    std::wstring counterPathSend = L"\\Network Interface(*)\\Bytes Sent/sec";
    std::wstring counterPathReceive = L"\\Network Interface(*)\\Bytes Received/sec";

    if (PdhAddEnglishCounter(hQuery_, counterPathSend.c_str(), 0, &hCounterSend_) != ERROR_SUCCESS) {
        Logger::log("Failed to add network send counter.");
        hCounterSend_ = nullptr;
    }

    if (PdhAddEnglishCounter(hQuery_, counterPathReceive.c_str(), 0, &hCounterReceive_) != ERROR_SUCCESS) {
        Logger::log("Failed to add network receive counter.");
        hCounterReceive_ = nullptr;
    }

    // Collect initial data
    PdhCollectQueryData(hQuery_);

    // Start the update thread
    update_thread_ = std::thread(&WindowsNetwork::updateLoop, this);
}

WindowsNetwork::~WindowsNetwork() {
    running_ = false;
    if (update_thread_.joinable()) {
        update_thread_.join();
    }

    if (hQuery_) {
        PdhCloseQuery(hQuery_);
        hQuery_ = nullptr;
    }

    WSACleanup(); // Clean up Winsock
}

void WindowsNetwork::updateLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        updateNetworkStats();
    }
}

void WindowsNetwork::updateNetworkStats() {
    if (!hQuery_ || !hCounterSend_ || !hCounterReceive_) {
        Logger::log("PDH counters not initialized for network monitoring.");
        return;
    }

    // Collect data
    PDH_STATUS pdhStatus = PdhCollectQueryData(hQuery_);
    if (pdhStatus != ERROR_SUCCESS) {
        Logger::log("Failed to collect PDH query data for network monitoring. Error code: " + std::to_string(pdhStatus));
        return;
    }

    // Variables for counter arrays
    PDH_FMT_COUNTERVALUE_ITEM_W* pItems = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwItemCount = 0;

    // Get Bytes Sent/sec for all instances
    pdhStatus = PdhGetFormattedCounterArrayW(hCounterSend_, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);
    if (pdhStatus == PDH_MORE_DATA) {
        pItems = (PDH_FMT_COUNTERVALUE_ITEM_W*)malloc(dwBufferSize);
        pdhStatus = PdhGetFormattedCounterArrayW(hCounterSend_, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);
    }
    if (pdhStatus != ERROR_SUCCESS) {
        Logger::log("Failed to get formatted counter array for Bytes Sent/sec. Error code: " + std::to_string(pdhStatus));
        if (pItems) free(pItems);
        return;
    }

    double totalBytesSentPerSec = 0.0;
    for (DWORD i = 0; i < dwItemCount; i++) {
        totalBytesSentPerSec += pItems[i].FmtValue.doubleValue;
    }
    free(pItems);

    // Get Bytes Received/sec for all instances
    pItems = NULL;
    dwBufferSize = 0;
    dwItemCount = 0;

    pdhStatus = PdhGetFormattedCounterArrayW(hCounterReceive_, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);
    if (pdhStatus == PDH_MORE_DATA) {
        pItems = (PDH_FMT_COUNTERVALUE_ITEM_W*)malloc(dwBufferSize);
        pdhStatus = PdhGetFormattedCounterArrayW(hCounterReceive_, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);
    }
    if (pdhStatus != ERROR_SUCCESS) {
        Logger::log("Failed to get formatted counter array for Bytes Received/sec. Error code: " + std::to_string(pdhStatus));
        if (pItems) free(pItems);
        return;
    }

    double totalBytesReceivedPerSec = 0.0;
    for (DWORD i = 0; i < dwItemCount; i++) {
        totalBytesReceivedPerSec += pItems[i].FmtValue.doubleValue;
    }
    free(pItems);

    float uploadRate = static_cast<float>(totalBytesSentPerSec) * 8 / (1024 * 1024);    // Convert to Mbps
    float downloadRate = static_cast<float>(totalBytesReceivedPerSec) * 8 / (1024 * 1024); // Convert to Mbps

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        currentUploadRate_ = uploadRate;
        currentDownloadRate_ = downloadRate;

        if (uploadRate > highestUploadRate_) {
            highestUploadRate_ = uploadRate;
        }
        if (downloadRate > highestDownloadRate_) {
            highestDownloadRate_ = downloadRate;
        }
    }
}


float WindowsNetwork::getUploadRate() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return currentUploadRate_/8.0;
}

float WindowsNetwork::getDownloadRate() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return currentDownloadRate_/8.0;
}

float WindowsNetwork::getHighestUploadRate() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return highestUploadRate_/8.0;
}

float WindowsNetwork::getHighestDownloadRate() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return highestDownloadRate_/8.0;
}

float WindowsNetwork::getTotalBandwidth() {
    // Implementing total bandwidth estimation
    ULONG bufferSize = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &bufferSize);

    IP_ADAPTER_ADDRESSES* pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
    if (pAddresses == NULL) {
        Logger::log("Error allocating memory for adapter addresses.");
        return 0.0f;
    }

    if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, pAddresses, &bufferSize) != NO_ERROR) {
        Logger::log("GetAdaptersAddresses failed.");
        free(pAddresses);
        return 0.0f;
    }

    float totalBandwidth = 0.0f;
    IP_ADAPTER_ADDRESSES* pCurrAddresses = pAddresses;

    while (pCurrAddresses) {
        // Exclude loopback and other non-physical adapters
        if (pCurrAddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK && pCurrAddresses->OperStatus == IfOperStatusUp) {
            totalBandwidth += static_cast<float>(pCurrAddresses->TransmitLinkSpeed) / (1000.0f * 1000.0f); // Convert to Mbps
        }
        pCurrAddresses = pCurrAddresses->Next;
    }

    free(pAddresses);
    return totalBandwidth;
}

float WindowsNetwork::getTotalUsedBandwidth() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return currentUploadRate_ + currentDownloadRate_;
}

std::string WindowsNetwork::getTopBandwidthProcess() {
    return "Feature not implemented";
}

#endif // _WIN32
