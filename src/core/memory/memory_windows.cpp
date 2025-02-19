// File: src/core/memory/memory_windows.cpp

#ifdef _WIN32

#include "memory_windows.h"
#include "utils/logger.h"
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <numeric>

#pragma comment(lib, "psapi.lib")

/**
 * @brief Constructs a new WindowsMemory object.
 */
WindowsMemory::WindowsMemory() {
    // Initialization if needed
}

/**
 * @brief Retrieves the total memory usage percentage.
 *
 * @return float Total memory usage as a percentage.
 */
float WindowsMemory::getTotalUsage() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        float usage = static_cast<float>(memStatus.dwMemoryLoad);

        // Update usage history
        usage_history_.push_back(usage);
        if (usage_history_.size() > max_history_size_) {
            usage_history_.erase(usage_history_.begin());
        }

        return usage;
    }
    else {
        Logger::log("Failed to get memory status.");
        return 0.0f;
    }
}

/**
 * @brief Retrieves the remaining RAM in megabytes.
 *
 * @return float Remaining RAM in MB.
 */
float WindowsMemory::getRemainingRAM() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        DWORDLONG freePhysMem = memStatus.ullAvailPhys;
        return static_cast<float>(freePhysMem) / (1024.0f * 1024.0f); // Convert to MB
    }
    else {
        Logger::log("Failed to get memory status.");
        return 0.0f;
    }
}

/**
 * @brief Retrieves the average memory usage percentage over the history.
 *
 * @return float Average memory usage as a percentage.
 */
float WindowsMemory::getAverageUsage() {
    if (usage_history_.empty()) {
        return 0.0f;
    }
    float sum = std::accumulate(usage_history_.begin(), usage_history_.end(), 0.0f);
    return sum / static_cast<float>(usage_history_.size());
}

/**
 * @brief Retrieves the name of the process consuming the most memory.
 *
 * @return std::string Name of the top memory-consuming process.
 */
std::string WindowsMemory::getMostUsingProcess() {
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        Logger::log("Failed to enumerate processes.");
        return "Unknown";
    }

    cProcesses = cbNeeded / sizeof(DWORD);

    SIZE_T maxMemUsage = 0;
    std::string maxMemProcess = "Unknown";

    for (unsigned int i = 0; i < cProcesses; i++) {
        if (aProcesses[i] == 0)
            continue;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
        if (hProcess) {
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                if (pmc.WorkingSetSize > maxMemUsage) {
                    maxMemUsage = pmc.WorkingSetSize;

                    TCHAR processName[MAX_PATH] = TEXT("<unknown>");
                    HMODULE hMod;
                    DWORD cbNeededMod;

                    if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeededMod)) {
                        GetModuleBaseName(hProcess, hMod, processName, sizeof(processName) / sizeof(TCHAR));
                    }

                    maxMemProcess = processName;
                }
            }
            CloseHandle(hProcess);
        }
    }

    return maxMemProcess;
}

#endif // _WIN32

