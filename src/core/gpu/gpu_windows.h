/**
 * @file gpu_windows.h
 * @brief Windows GPU monitoring using DXGI and NVML.
 */

#pragma once

#ifdef _WIN32

#include "gpu_common.h"

#include <windows.h>
#include <dxgi1_4.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "dxgi.lib")

using nvmlReturn_t     = unsigned int;
using nvmlDevice_t     = void*;
using nvmlTemperatureSensors_t = unsigned int;
using nvmlClockType_t  = unsigned int;

/// @brief NVML GPU/memory utilization percentages.
struct nvmlUtilization_t {
    unsigned int gpu;    ///< GPU core utilization percent
    unsigned int memory; ///< Memory controller utilization percent
};

/// @brief NVML video memory sizes in bytes.
struct nvmlMemory_t {
    unsigned long long total; ///< Total installed memory
    unsigned long long free;  ///< Unallocated memory
    unsigned long long used;  ///< Allocated memory
};

static constexpr nvmlReturn_t              NVML_SUCCESS                       = 0;
static constexpr nvmlTemperatureSensors_t  NVML_TEMPERATURE_GPU               = 0;
static constexpr nvmlClockType_t           NVML_CLOCK_GRAPHICS                = 0;
static constexpr nvmlClockType_t           NVML_CLOCK_MEM                     = 2;

using pfn_nvmlInit_v2                = nvmlReturn_t (*)();
using pfn_nvmlShutdown               = nvmlReturn_t (*)();
using pfn_nvmlDeviceGetCount_v2      = nvmlReturn_t (*)(unsigned int*);
using pfn_nvmlDeviceGetHandleByIndex_v2 = nvmlReturn_t (*)(unsigned int, nvmlDevice_t*);
using pfn_nvmlDeviceGetName          = nvmlReturn_t (*)(nvmlDevice_t, char*, unsigned int);
using pfn_nvmlDeviceGetTemperature   = nvmlReturn_t (*)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int*);
using pfn_nvmlDeviceGetUtilizationRates = nvmlReturn_t (*)(nvmlDevice_t, nvmlUtilization_t*);
using pfn_nvmlDeviceGetMemoryInfo    = nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t*);
using pfn_nvmlDeviceGetPowerUsage    = nvmlReturn_t (*)(nvmlDevice_t, unsigned int*);
using pfn_nvmlDeviceGetFanSpeed      = nvmlReturn_t (*)(nvmlDevice_t, unsigned int*);
using pfn_nvmlDeviceGetClockInfo     = nvmlReturn_t (*)(nvmlDevice_t, nvmlClockType_t, unsigned int*);
using pfn_nvmlDeviceGetDriverVersion = nvmlReturn_t (*)(char*, unsigned int);

/// @brief Cached information about a DXGI adapter.
struct DxgiAdapterEntry {
    std::string  name;
    std::string  vendor;              ///< "NVIDIA", "AMD", "Intel", or "Unknown"
    uint32_t     vendorId  = 0;       ///< PCI vendor ID
    uint64_t     vramTotal = 0;       ///< Dedicated video memory in bytes
    LUID         luid{};              ///< Adapter LUID for cross-API correlation
};

/**
 * @brief Windows GPU monitor using DXGI for all vendors and NVML for NVIDIA.
 */
class WindowsGPU : public GPU {
public:
    WindowsGPU();
    ~WindowsGPU() override;

    void        update()                override;
    GpuSnapshot snapshot() const        override;

private:
    /// @brief Dynamically load nvml.dll and resolve function pointers.
    void loadNvml();
    /// @brief Shut down NVML and free the library.
    void unloadNvml();
    /**
     * @brief Query all NVIDIA GPUs via NVML.
     * @param out Vector to append GpuInfo entries to.
     */
    void queryNvml(std::vector<GpuInfo>& out);

    HMODULE nvmlLib_ = nullptr;        ///< Handle to nvml.dll
    bool    nvmlSupported_ = false;    ///< True if NVML initialized successfully

    pfn_nvmlInit_v2                   fnInit_          = nullptr;
    pfn_nvmlShutdown                  fnShutdown_      = nullptr;
    pfn_nvmlDeviceGetCount_v2         fnGetCount_      = nullptr;
    pfn_nvmlDeviceGetHandleByIndex_v2 fnGetHandle_     = nullptr;
    pfn_nvmlDeviceGetName             fnGetName_       = nullptr;
    pfn_nvmlDeviceGetTemperature      fnGetTemp_       = nullptr;
    pfn_nvmlDeviceGetUtilizationRates fnGetUtil_       = nullptr;
    pfn_nvmlDeviceGetMemoryInfo       fnGetMemInfo_    = nullptr;
    pfn_nvmlDeviceGetPowerUsage       fnGetPower_      = nullptr;
    pfn_nvmlDeviceGetFanSpeed         fnGetFan_        = nullptr;
    pfn_nvmlDeviceGetClockInfo        fnGetClock_      = nullptr;
    pfn_nvmlDeviceGetDriverVersion    fnGetDriver_     = nullptr;
    unsigned int nvmlDeviceCount_ = 0; ///< Number of NVIDIA devices found

    /// @brief Enumerate all non-software DXGI adapters (called once).
    void enumerateDXGI();
    /**
     * @brief Query VRAM usage for non-NVIDIA GPUs via DXGI.
     * @param out Vector to append GpuInfo entries to.
     */
    void queryDXGIMemory(std::vector<GpuInfo>& out);

    bool                          dxgiEnumerated_ = false; ///< Whether DXGI enumeration has run
    std::vector<DxgiAdapterEntry> dxgiAdapters_;           ///< Cached adapter list

    mutable std::mutex mutex_;   ///< Guards current_
    GpuSnapshot        current_; ///< Latest snapshot
};

#endif // _WIN32
