// File: src/core/cpu/cpu_windows.cpp

#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "cpu_windows.h"
#include "utils/logger.h"
#include <windows.h>
#include <iostream>
#include <vector>
#include <numeric>
#include <tlhelp32.h>
#include <Wbemidl.h>
#include <pdhmsg.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "pdh.lib")
/**
 * @brief Constructor for WindowsCPU.
 *
 * Initializes variables and starts the CPU usage update thread.
 */
WindowsCPU::WindowsCPU()
    : highest_usage_(0.0f), running_(true), current_usage_(0.0f)
{
    // Initialize previous times
    FILETIME idleTime, kernelTime, userTime;
    GetSystemTimes(&idleTime_, &kernelTime_, &userTime_);
    prev_idle_time_ = fileTimeToInt64(idleTime_);
    prev_kernel_time_ = fileTimeToInt64(kernelTime_);
    prev_user_time_ = fileTimeToInt64(userTime_);

    // Start the update thread
    update_thread_ = std::thread(&WindowsCPU::updateLoop, this);
}

/**
 * @brief Destructor for WindowsCPU.
 *
 * Stops the update thread.
 */
WindowsCPU::~WindowsCPU() {
    running_ = false;
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}

/**
 * @brief Thread function to periodically update CPU usage.
 *
 * Collects CPU usage data every interval and updates internal metrics.
 */
void WindowsCPU::updateLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        float usage = calculateCPUUsage();
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            current_usage_ = usage;

            if (usage > highest_usage_) {
                highest_usage_ = usage;
            }

            // Update usage history
            usage_history_.push_back(usage);
            if (usage_history_.size() > max_history_size_) {
                usage_history_.erase(usage_history_.begin());
            }
        }
    }
}

/**
 * @brief Calculates the CPU usage using GetSystemTimes.
 * @return CPU usage as a percentage.
 */
float WindowsCPU::calculateCPUUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULONGLONG idle = fileTimeToInt64(idleTime);
        ULONGLONG kernel = fileTimeToInt64(kernelTime);
        ULONGLONG user = fileTimeToInt64(userTime);

        ULONGLONG idleDiff = idle - prev_idle_time_;
        ULONGLONG kernelDiff = kernel - prev_kernel_time_;
        ULONGLONG userDiff = user - prev_user_time_;

        ULONGLONG totalSystem = kernelDiff + userDiff;
        ULONGLONG totalIdle = idleDiff;

        float cpuUsage = 0.0f;
        if (totalSystem > 0) {
            cpuUsage = (float)(totalSystem - totalIdle) * 100.0f / totalSystem;
        }

        prev_idle_time_ = idle;
        prev_kernel_time_ = kernel;
        prev_user_time_ = user;

        return cpuUsage;
    } else {
        Logger::log("GetSystemTimes failed.");
        return 0.0f;
    }
}

/**
 * @brief Helper function to convert FILETIME to ULONGLONG.
 * @param ft FILETIME structure.
 * @return ULONGLONG representation.
 */
ULONGLONG WindowsCPU::fileTimeToInt64(const FILETIME &ft) {
    return (((ULONGLONG)(ft.dwHighDateTime)) << 32) | ft.dwLowDateTime;
}

/**
 * @brief Retrieves the total CPU usage percentage.
 * @return Total CPU usage as a percentage.
 */
float WindowsCPU::getTotalUsage() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_usage_;
}

/**
 * @brief Retrieves the CPU clock frequency in GHz.
 * @return CPU clock frequency in GHz.
 */
float WindowsCPU::getClockFrequency() {
    PDH_HQUERY hQuery = NULL;
    PDH_HCOUNTER hCounter;
    PDH_FMT_COUNTERVALUE counterVal;

    // Open a PDH query
    if (PdhOpenQuery(NULL, NULL, &hQuery) != ERROR_SUCCESS) {
        Logger::log("Failed to open PDH query for CPU frequency.");
        return 0.0f;
    }

    // Add the Processor Frequency counter
    if (PdhAddCounter(hQuery, L"\\Processor Information(_Total)\\Processor Frequency", NULL, &hCounter) != ERROR_SUCCESS) {
        Logger::log("Failed to add PDH counter for CPU frequency.");
        PdhCloseQuery(hQuery);
        return 0.0f;
    }

    // Collect data
    PdhCollectQueryData(hQuery);
    Sleep(1000); // Wait a second to get a valid sample
    PdhCollectQueryData(hQuery);

    // Get the formatted counter value
    if (PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, NULL, &counterVal) != ERROR_SUCCESS) {
        Logger::log("Failed to get formatted counter value for CPU frequency.");
        PdhCloseQuery(hQuery);
        return 0.0f;
    }

    // Cleanup
    PdhCloseQuery(hQuery);

    // Convert MHz to GHz
    return static_cast<float>(counterVal.doubleValue) / 1000.0f;
}


/**
 * @brief Retrieves the number of threads currently used by the current process.
 * @return Number of threads used by the current process.
 */
int WindowsCPU::getUsedThreads() {
    DWORD currentProcessId = GetCurrentProcessId();
    HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
    THREADENTRY32 te32;
    int threadCount = 0;

    // Take a snapshot of all running threads
    hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE)
        return 0;

    te32.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(hThreadSnap, &te32))
    {
        CloseHandle(hThreadSnap);          // clean the snapshot object
        return 0;
    }

    do
    {
        if (te32.th32OwnerProcessID == currentProcessId)
            threadCount++;
    } while (Thread32Next(hThreadSnap, &te32));

    CloseHandle(hThreadSnap);
    return threadCount;
}

/**
 * @brief Retrieves the total number of threads in the system.
 * @return Total number of threads in the system.
 */
int WindowsCPU::getTotalThreads() {
    HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
    THREADENTRY32 te32;
    int threadCount = 0;

    // Take a snapshot of all running threads
    hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE)
        return 0;

    te32.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(hThreadSnap, &te32))
    {
        CloseHandle(hThreadSnap);          // clean the snapshot object
        return 0;
    }

    do
    {
        threadCount++;
    } while (Thread32Next(hThreadSnap, &te32));

    CloseHandle(hThreadSnap);
    return threadCount;
}

/**
 * @brief Retrieves the highest CPU usage recorded.
 * @return Highest CPU usage as a percentage.
 */
float WindowsCPU::getHighestUsage() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return highest_usage_;
}

/**
 * @brief Retrieves the average CPU usage over time.
 * @return Average CPU usage as a percentage.
 */
float WindowsCPU::getAverageUsage() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (usage_history_.empty()) {
        return 0.0f;
    }
    float sum = std::accumulate(usage_history_.begin(), usage_history_.end(), 0.0f);
    return sum / static_cast<float>(usage_history_.size());
}

#endif // _WIN32
