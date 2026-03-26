/**
 * @file system_info.cpp
 * @brief Platform-aware system information implementation.
 *
 * Uses #ifdef _WIN32 / __linux__ to select the right OS calls.
 * Static info is gathered once in the constructor; dynamic info
 * (uptime) is refreshed on each update().
 */

#include "system_info.h"

#include <string>
#include <algorithm>
#include <cstring>

// ---------------------------------------------------------------------------
// Platform headers
// ---------------------------------------------------------------------------

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>   // __cpuid

#pragma comment(lib, "advapi32.lib")

#elif defined(__linux__)

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <set>

#endif

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SystemInfo::SystemInfo() {
    queryStatic();
    queryDynamic();
}

// ---------------------------------------------------------------------------
// update() / snapshot()
// ---------------------------------------------------------------------------

void SystemInfo::update() {
    queryDynamic();
}

SystemInfoSnapshot SystemInfo::snapshot() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return data_;
}

// ---------------------------------------------------------------------------
// queryStatic()  -- called once
// ---------------------------------------------------------------------------

void SystemInfo::queryStatic() {
    std::lock_guard<std::mutex> lock(mtx_);

#ifdef _WIN32

    // --- OS Name / Version ---------------------------------------------------
    // Read the registry for a user-friendly product name and version.
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                          "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                          0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buf[256]{};
            DWORD bufLen = sizeof(buf);
            DWORD type   = 0;

            // ProductName (e.g. "Windows 11 Pro")
            if (RegQueryValueExA(hKey, "ProductName", nullptr, &type,
                                 reinterpret_cast<LPBYTE>(buf), &bufLen) == ERROR_SUCCESS
                && type == REG_SZ) {
                data_.osName = buf;
            } else {
                data_.osName = "Windows";
            }

            // DisplayVersion (e.g. "23H2")
            bufLen = sizeof(buf);
            if (RegQueryValueExA(hKey, "DisplayVersion", nullptr, &type,
                                 reinterpret_cast<LPBYTE>(buf), &bufLen) == ERROR_SUCCESS
                && type == REG_SZ) {
                data_.osVersion = buf;
            } else {
                // Fallback: CurrentBuildNumber
                bufLen = sizeof(buf);
                if (RegQueryValueExA(hKey, "CurrentBuildNumber", nullptr, &type,
                                     reinterpret_cast<LPBYTE>(buf), &bufLen) == ERROR_SUCCESS
                    && type == REG_SZ) {
                    data_.osVersion = std::string("Build ") + buf;
                }
            }

            RegCloseKey(hKey);
        } else {
            data_.osName = "Windows";
        }
    }

    // kernelVersion -- use major.minor.build from RtlGetVersion via OSVERSIONINFOEXA
    {
        // RtlGetVersion is in ntdll and avoids the compatibility shim that
        // caps GetVersionEx at 6.2. We dynamically load it.
        typedef LONG(WINAPI* RtlGetVersionPtr)(OSVERSIONINFOEXA*);
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            auto pRtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
                GetProcAddress(hNtdll, "RtlGetVersion"));
            if (pRtlGetVersion) {
                OSVERSIONINFOEXA osvi{};
                osvi.dwOSVersionInfoSize = sizeof(osvi);
                if (pRtlGetVersion(&osvi) == 0 /* STATUS_SUCCESS */) {
                    data_.kernelVersion = std::to_string(osvi.dwMajorVersion) + "."
                                        + std::to_string(osvi.dwMinorVersion) + "."
                                        + std::to_string(osvi.dwBuildNumber);
                }
            }
        }
    }

    // --- Hostname ------------------------------------------------------------
    {
        char buf[MAX_COMPUTERNAME_LENGTH + 1]{};
        DWORD len = sizeof(buf);
        if (GetComputerNameA(buf, &len)) {
            data_.hostname = std::string(buf, len);
        }
    }

    // --- Architecture --------------------------------------------------------
    {
        SYSTEM_INFO si{};
        GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: data_.arch = "x86_64";  break;
            case PROCESSOR_ARCHITECTURE_ARM64: data_.arch = "ARM64";   break;
            case PROCESSOR_ARCHITECTURE_INTEL: data_.arch = "x86";     break;
            case PROCESSOR_ARCHITECTURE_ARM:   data_.arch = "ARM";     break;
            default:                           data_.arch = "Unknown"; break;
        }
    }

    // --- CPU Model -----------------------------------------------------------
    {
        // Try __cpuid brand string (leaves 0x80000002..0x80000004).
        int cpuInfo[4]{};
        __cpuid(cpuInfo, 0x80000000);
        unsigned int maxExt = static_cast<unsigned int>(cpuInfo[0]);
        if (maxExt >= 0x80000004) {
            char brand[49]{};
            for (unsigned int leaf = 0x80000002; leaf <= 0x80000004; ++leaf) {
                __cpuid(cpuInfo, static_cast<int>(leaf));
                std::memcpy(brand + (leaf - 0x80000002) * 16, cpuInfo, 16);
            }
            brand[48] = '\0';
            // Trim leading spaces.
            const char* p = brand;
            while (*p == ' ') ++p;
            data_.cpuModel = p;
        } else {
            // Fallback: registry.
            HKEY hKey = nullptr;
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                              "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                              0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                char buf[256]{};
                DWORD bufLen = sizeof(buf);
                DWORD type   = 0;
                if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, &type,
                                     reinterpret_cast<LPBYTE>(buf), &bufLen) == ERROR_SUCCESS
                    && type == REG_SZ) {
                    data_.cpuModel = buf;
                }
                RegCloseKey(hKey);
            }
        }
    }

    // --- Cores ---------------------------------------------------------------
    {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        data_.cpuLogicalCores = static_cast<int>(si.dwNumberOfProcessors);

        // Physical cores via GetLogicalProcessorInformation.
        DWORD len = 0;
        GetLogicalProcessorInformation(nullptr, &len);
        if (len > 0) {
            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buf(
                len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            if (GetLogicalProcessorInformation(buf.data(), &len)) {
                int phys = 0;
                for (auto& info : buf) {
                    if (info.Relationship == RelationProcessorCore) ++phys;
                }
                data_.cpuPhysicalCores = phys > 0 ? phys : data_.cpuLogicalCores;
            } else {
                data_.cpuPhysicalCores = (data_.cpuLogicalCores > 1)
                                       ? data_.cpuLogicalCores / 2 : 1;
            }
        } else {
            data_.cpuPhysicalCores = (data_.cpuLogicalCores > 1)
                                   ? data_.cpuLogicalCores / 2 : 1;
        }
    }

    // --- Cache sizes ---------------------------------------------------------
    {
        DWORD bufLen = 0;
        GetLogicalProcessorInformation(nullptr, &bufLen);
        if (bufLen > 0) {
            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buf(
                bufLen / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            if (GetLogicalProcessorInformation(buf.data(), &bufLen)) {
                for (auto& entry : buf) {
                    if (entry.Relationship == RelationCache) {
                        auto& cache = entry.Cache;
                        uint32_t sizeKB = cache.Size / 1024;
                        switch (cache.Level) {
                            case 1: data_.l1CacheKB += sizeKB; break;
                            case 2: data_.l2CacheKB += sizeKB; break;
                            case 3: data_.l3CacheKB += sizeKB; break;
                        }
                    }
                }
            }
        }
    }

    // --- Total RAM -----------------------------------------------------------
    {
        MEMORYSTATUSEX memStat{};
        memStat.dwLength = sizeof(memStat);
        if (GlobalMemoryStatusEx(&memStat)) {
            data_.totalRAM = static_cast<uint64_t>(memStat.ullTotalPhys);
        }
    }

#elif defined(__linux__)

    // --- OS Name / Version (from /etc/os-release) ----------------------------
    {
        std::ifstream f("/etc/os-release");
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                if (line.compare(0, 12, "PRETTY_NAME=") == 0) {
                    std::string val = line.substr(12);
                    // Remove surrounding quotes.
                    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                        val = val.substr(1, val.size() - 2);
                    }
                    data_.osName = val;
                } else if (line.compare(0, 11, "VERSION_ID=") == 0) {
                    std::string val = line.substr(11);
                    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                        val = val.substr(1, val.size() - 2);
                    }
                    data_.osVersion = val;
                }
            }
        }
        if (data_.osName.empty()) data_.osName = "Linux";
    }

    // --- Kernel version, hostname, architecture (uname) ----------------------
    {
        struct utsname un{};
        if (uname(&un) == 0) {
            data_.kernelVersion = std::string(un.sysname) + " " + un.release;
            data_.hostname      = un.nodename;
            data_.arch          = un.machine;
        }
    }

    // --- CPU Model (from /proc/cpuinfo) --------------------------------------
    {
        std::ifstream f("/proc/cpuinfo");
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                if (line.compare(0, 10, "model name") == 0) {
                    auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        std::string val = line.substr(pos + 1);
                        // Trim leading whitespace.
                        auto start = val.find_first_not_of(" \t");
                        if (start != std::string::npos)
                            val = val.substr(start);
                        data_.cpuModel = val;
                    }
                    break; // Only need the first one.
                }
            }
        }
    }

    // --- Cores ---------------------------------------------------------------
    {
        data_.cpuLogicalCores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));

        // Physical cores: count unique (physical id, core id) pairs in /proc/cpuinfo.
        std::set<std::string> uniqueCores;
        std::ifstream f("/proc/cpuinfo");
        if (f.is_open()) {
            std::string line;
            std::string physId, coreId;
            while (std::getline(f, line)) {
                if (line.compare(0, 11, "physical id") == 0) {
                    auto pos = line.find(':');
                    if (pos != std::string::npos)
                        physId = line.substr(pos + 1);
                } else if (line.compare(0, 7, "core id") == 0) {
                    auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        coreId = line.substr(pos + 1);
                        uniqueCores.insert(physId + ":" + coreId);
                    }
                }
            }
        }
        data_.cpuPhysicalCores = uniqueCores.empty()
                               ? data_.cpuLogicalCores
                               : static_cast<int>(uniqueCores.size());
    }

    // --- Cache sizes ---------------------------------------------------------
    {
        // Enumerate /sys/devices/system/cpu/cpu0/cache/index*
        for (int idx = 0; idx < 10; ++idx) {
            std::string base = "/sys/devices/system/cpu/cpu0/cache/index"
                             + std::to_string(idx);
            std::ifstream fLevel(base + "/level");
            std::ifstream fSize(base + "/size");
            if (!fLevel.is_open() || !fSize.is_open()) break;

            int level = 0;
            fLevel >> level;

            std::string sizeStr;
            fSize >> sizeStr;
            // Parse e.g. "32K", "1024K", "16M"
            uint32_t sizeKB = 0;
            if (!sizeStr.empty()) {
                char suffix = sizeStr.back();
                std::string numPart = sizeStr.substr(0, sizeStr.size() - 1);
                uint32_t num = static_cast<uint32_t>(std::stoul(numPart));
                if (suffix == 'K' || suffix == 'k') {
                    sizeKB = num;
                } else if (suffix == 'M' || suffix == 'm') {
                    sizeKB = num * 1024;
                } else {
                    // No suffix or unknown; treat as bytes
                    sizeKB = static_cast<uint32_t>(std::stoul(sizeStr)) / 1024;
                }
            }

            switch (level) {
                case 1: data_.l1CacheKB += sizeKB; break;
                case 2: data_.l2CacheKB += sizeKB; break;
                case 3: data_.l3CacheKB += sizeKB; break;
            }
        }
    }

    // --- Total RAM -----------------------------------------------------------
    {
        struct sysinfo si{};
        if (sysinfo(&si) == 0) {
            data_.totalRAM = static_cast<uint64_t>(si.totalram)
                           * static_cast<uint64_t>(si.mem_unit);
        }
    }

#endif
}

// ---------------------------------------------------------------------------
// queryDynamic()  -- called on every update()
// ---------------------------------------------------------------------------

void SystemInfo::queryDynamic() {
    std::lock_guard<std::mutex> lock(mtx_);

#ifdef _WIN32

    data_.uptimeSeconds = static_cast<uint64_t>(GetTickCount64() / 1000ULL);

#elif defined(__linux__)

    {
        struct sysinfo si{};
        if (sysinfo(&si) == 0) {
            data_.uptimeSeconds = static_cast<uint64_t>(si.uptime);
        }
    }

#endif
}
