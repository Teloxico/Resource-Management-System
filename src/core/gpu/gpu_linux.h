/**
 * @file gpu_linux.h
 * @brief Linux GPU monitoring using NVML, amdgpu sysfs, and i915 sysfs.
 */

#pragma once

#ifdef __linux__

#include "gpu_common.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

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

/**
 * @brief Linux GPU monitor supporting NVIDIA (NVML), AMD (amdgpu), and Intel (i915/xe).
 */
class LinuxGPU : public GPU {
public:
    LinuxGPU();
    ~LinuxGPU() override;

    void        update()                override;
    GpuSnapshot snapshot() const        override;

private:
    /// @brief Dynamically load libnvidia-ml.so and resolve function pointers.
    void loadNvml();
    /// @brief Shut down NVML and close the shared library.
    void unloadNvml();
    /**
     * @brief Query all NVIDIA GPUs via NVML.
     * @param out Vector to append GpuInfo entries to.
     */
    void queryNvml(std::vector<GpuInfo>& out);

    void* nvmlLib_           = nullptr; ///< dlopen handle to libnvidia-ml.so
    bool  nvmlSupported_     = false;   ///< True if NVML initialized successfully
    unsigned int nvmlDeviceCount_ = 0;  ///< Number of NVIDIA devices found

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

    /**
     * @brief Query AMD GPUs via amdgpu sysfs.
     * @param out Vector to append GpuInfo entries to.
     */
    void queryAmdgpu(std::vector<GpuInfo>& out);

    /**
     * @brief Query Intel GPUs via i915/xe sysfs.
     * @param out Vector to append GpuInfo entries to.
     */
    void queryIntel(std::vector<GpuInfo>& out);

    /**
     * @brief Read a single integer from a sysfs file.
     * @param path Absolute path to the sysfs file.
     * @return The parsed value, or -1 on failure.
     */
    static int64_t readSysfsInt(const std::string& path);

    /**
     * @brief Read the first line from a sysfs file.
     * @param path Absolute path to the sysfs file.
     * @return Trimmed line contents, or empty string on failure.
     */
    static std::string readSysfsString(const std::string& path);

    /**
     * @brief Find the first hwmon subdirectory under a device path.
     * @param devicePath Path to the device in sysfs.
     * @return Full path to the hwmon directory, or empty on failure.
     */
    static std::string findHwmonDir(const std::string& devicePath);

    /**
     * @brief Parse the active frequency from a DPM frequency file.
     * @param path Path to pp_dpm_sclk or pp_dpm_mclk.
     * @return Active frequency in MHz, or 0 if not found.
     */
    static float parseActiveDpmFreq(const std::string& path);

    mutable std::mutex mutex_;   ///< Guards current_
    GpuSnapshot        current_; ///< Latest snapshot
};

#endif // __linux__
