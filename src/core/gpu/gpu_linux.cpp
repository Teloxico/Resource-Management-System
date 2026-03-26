/**
 * @file gpu_linux.cpp
 * @brief Linux GPU monitoring implementation (NVML, amdgpu sysfs, i915 sysfs).
 */

#ifdef __linux__

#include "gpu_linux.h"

#include <dlfcn.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

LinuxGPU::LinuxGPU() {
    loadNvml();
}

LinuxGPU::~LinuxGPU() {
    unloadNvml();
}

int64_t LinuxGPU::readSysfsInt(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return -1;
    int64_t val = -1;
    f >> val;
    return f.fail() ? -1 : val;
}

std::string LinuxGPU::readSysfsString(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
        line.pop_back();
    return line;
}

std::string LinuxGPU::findHwmonDir(const std::string& devicePath) {
    std::string hwmonBase = devicePath + "/hwmon";
    try {
        for (const auto& entry : fs::directory_iterator(hwmonBase)) {
            if (entry.is_directory()) {
                return entry.path().string();
            }
        }
    } catch (...) {}
    return {};
}

float LinuxGPU::parseActiveDpmFreq(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return 0.0f;

    std::string line;
    while (std::getline(f, line)) {
        if (line.find('*') != std::string::npos) {
            auto colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::istringstream ss(line.substr(colonPos + 1));
                int mhz = 0;
                ss >> mhz;
                if (!ss.fail() && mhz > 0)
                    return static_cast<float>(mhz);
            }
        }
    }
    return 0.0f;
}

void LinuxGPU::loadNvml() {
    nvmlLib_ = dlopen("libnvidia-ml.so.1", RTLD_NOW);
    if (!nvmlLib_) nvmlLib_ = dlopen("libnvidia-ml.so", RTLD_NOW);
    if (!nvmlLib_) { nvmlSupported_ = false; return; }

    fnInit_       = reinterpret_cast<pfn_nvmlInit_v2>(dlsym(nvmlLib_, "nvmlInit_v2"));
    fnShutdown_   = reinterpret_cast<pfn_nvmlShutdown>(dlsym(nvmlLib_, "nvmlShutdown"));
    fnGetCount_   = reinterpret_cast<pfn_nvmlDeviceGetCount_v2>(dlsym(nvmlLib_, "nvmlDeviceGetCount_v2"));
    fnGetHandle_  = reinterpret_cast<pfn_nvmlDeviceGetHandleByIndex_v2>(dlsym(nvmlLib_, "nvmlDeviceGetHandleByIndex_v2"));
    fnGetName_    = reinterpret_cast<pfn_nvmlDeviceGetName>(dlsym(nvmlLib_, "nvmlDeviceGetName"));
    fnGetTemp_    = reinterpret_cast<pfn_nvmlDeviceGetTemperature>(dlsym(nvmlLib_, "nvmlDeviceGetTemperature"));
    fnGetUtil_    = reinterpret_cast<pfn_nvmlDeviceGetUtilizationRates>(dlsym(nvmlLib_, "nvmlDeviceGetUtilizationRates"));
    fnGetMemInfo_ = reinterpret_cast<pfn_nvmlDeviceGetMemoryInfo>(dlsym(nvmlLib_, "nvmlDeviceGetMemoryInfo"));
    fnGetPower_   = reinterpret_cast<pfn_nvmlDeviceGetPowerUsage>(dlsym(nvmlLib_, "nvmlDeviceGetPowerUsage"));
    fnGetFan_     = reinterpret_cast<pfn_nvmlDeviceGetFanSpeed>(dlsym(nvmlLib_, "nvmlDeviceGetFanSpeed"));
    fnGetClock_   = reinterpret_cast<pfn_nvmlDeviceGetClockInfo>(dlsym(nvmlLib_, "nvmlDeviceGetClockInfo"));
    fnGetDriver_  = reinterpret_cast<pfn_nvmlDeviceGetDriverVersion>(dlsym(nvmlLib_, "nvmlSystemGetDriverVersion"));

    if (!fnInit_ || !fnShutdown_ || !fnGetCount_ || !fnGetHandle_) {
        dlclose(nvmlLib_); nvmlLib_ = nullptr; nvmlSupported_ = false; return;
    }
    if (fnInit_() != NVML_SUCCESS) {
        dlclose(nvmlLib_); nvmlLib_ = nullptr; nvmlSupported_ = false; return;
    }

    unsigned int count = 0;
    if (fnGetCount_ && fnGetCount_(&count) == NVML_SUCCESS)
        nvmlDeviceCount_ = count;

    nvmlSupported_ = (nvmlDeviceCount_ > 0);
}

void LinuxGPU::unloadNvml() {
    if (fnShutdown_) fnShutdown_();
    if (nvmlLib_) { dlclose(nvmlLib_); nvmlLib_ = nullptr; }
    nvmlSupported_ = false;
}

void LinuxGPU::queryNvml(std::vector<GpuInfo>& out) {
    std::string driverStr;
    if (fnGetDriver_) {
        char buf[128]{};
        if (fnGetDriver_(buf, sizeof(buf)) == NVML_SUCCESS)
            driverStr = buf;
    }

    for (unsigned int i = 0; i < nvmlDeviceCount_; ++i) {
        nvmlDevice_t dev = nullptr;
        if (!fnGetHandle_ || fnGetHandle_(i, &dev) != NVML_SUCCESS) continue;

        GpuInfo info;
        info.available = true;
        info.vendor    = "NVIDIA";
        info.driver    = driverStr;

        if (fnGetName_) {
            char nameBuf[128]{};
            if (fnGetName_(dev, nameBuf, sizeof(nameBuf)) == NVML_SUCCESS)
                info.name = nameBuf;
        }
        if (fnGetUtil_) {
            nvmlUtilization_t util{};
            if (fnGetUtil_(dev, &util) == NVML_SUCCESS)
                info.utilization = static_cast<float>(util.gpu);
        }
        if (fnGetMemInfo_) {
            nvmlMemory_t mem{};
            if (fnGetMemInfo_(dev, &mem) == NVML_SUCCESS) {
                info.memoryTotal = mem.total;
                info.memoryUsed  = mem.used;
                if (mem.total > 0)
                    info.memoryPercent = static_cast<float>(mem.used) * 100.0f
                                       / static_cast<float>(mem.total);
            }
        }
        if (fnGetTemp_) {
            unsigned int temp = 0;
            if (fnGetTemp_(dev, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS)
                info.temperature = static_cast<float>(temp);
        }
        if (fnGetPower_) {
            unsigned int mw = 0;
            if (fnGetPower_(dev, &mw) == NVML_SUCCESS)
                info.powerWatts = static_cast<float>(mw) / 1000.0f;
        }
        if (fnGetFan_) {
            unsigned int fan = 0;
            if (fnGetFan_(dev, &fan) == NVML_SUCCESS)
                info.fanPercent = static_cast<float>(fan);
        }
        if (fnGetClock_) {
            unsigned int mhz = 0;
            if (fnGetClock_(dev, NVML_CLOCK_GRAPHICS, &mhz) == NVML_SUCCESS)
                info.clockMHz = static_cast<float>(mhz);
            mhz = 0;
            if (fnGetClock_(dev, NVML_CLOCK_MEM, &mhz) == NVML_SUCCESS)
                info.memClockMHz = static_cast<float>(mhz);
        }

        out.push_back(std::move(info));
    }
}

void LinuxGPU::queryAmdgpu(std::vector<GpuInfo>& out) {
    try {
        for (const auto& card : fs::directory_iterator("/sys/class/drm")) {
            std::string cardName = card.path().filename().string();

            if (cardName.compare(0, 4, "card") != 0) continue;
            if (cardName.find('-') != std::string::npos) continue;

            std::string devPath = card.path().string() + "/device";

            std::string vendorStr = readSysfsString(devPath + "/vendor");
            if (vendorStr.find("0x1002") == std::string::npos) continue;

            {
                std::string driverLink = devPath + "/driver";
                try {
                    auto target = fs::read_symlink(driverLink).filename().string();
                    if (target != "amdgpu") continue;
                } catch (...) { continue; }
            }

            GpuInfo info;
            info.available = true;
            info.vendor    = "AMD";

            info.name = readSysfsString(devPath + "/product_name");
            if (info.name.empty()) {
                std::string devId = readSysfsString(devPath + "/device");
                if (!devId.empty())
                    info.name = "AMD GPU (" + devId + ")";
                else
                    info.name = "AMD GPU";
            }

            {
                int64_t val = readSysfsInt(devPath + "/gpu_busy_percent");
                if (val >= 0)
                    info.utilization = static_cast<float>(val);
            }

            {
                int64_t total = readSysfsInt(devPath + "/mem_info_vram_total");
                int64_t used  = readSysfsInt(devPath + "/mem_info_vram_used");
                if (total > 0) {
                    info.memoryTotal = static_cast<uint64_t>(total);
                    info.memoryUsed  = (used > 0) ? static_cast<uint64_t>(used) : 0;
                    info.memoryPercent = static_cast<float>(info.memoryUsed) * 100.0f
                                       / static_cast<float>(info.memoryTotal);
                }
            }

            std::string hwmon = findHwmonDir(devPath);
            if (!hwmon.empty()) {
                int64_t millideg = readSysfsInt(hwmon + "/temp1_input");
                if (millideg > 0)
                    info.temperature = static_cast<float>(millideg) / 1000.0f;

                int64_t uw = readSysfsInt(hwmon + "/power1_average");
                if (uw > 0)
                    info.powerWatts = static_cast<float>(uw) / 1000000.0f;

                int64_t pwm = readSysfsInt(hwmon + "/pwm1");
                if (pwm >= 0)
                    info.fanPercent = static_cast<float>(pwm) * 100.0f / 255.0f;
            }

            info.clockMHz    = parseActiveDpmFreq(devPath + "/pp_dpm_sclk");
            info.memClockMHz = parseActiveDpmFreq(devPath + "/pp_dpm_mclk");

            info.driver = readSysfsString("/sys/module/amdgpu/version");
            if (info.driver.empty()) info.driver = "amdgpu";

            out.push_back(std::move(info));
        }
    } catch (...) {
    }
}

void LinuxGPU::queryIntel(std::vector<GpuInfo>& out) {
    try {
        for (const auto& card : fs::directory_iterator("/sys/class/drm")) {
            std::string cardName = card.path().filename().string();
            if (cardName.compare(0, 4, "card") != 0) continue;
            if (cardName.find('-') != std::string::npos) continue;

            std::string cardPath = card.path().string();
            std::string devPath  = cardPath + "/device";

            std::string vendorStr = readSysfsString(devPath + "/vendor");
            if (vendorStr.find("0x8086") == std::string::npos) continue;

            std::string driverName;
            {
                std::string driverLink = devPath + "/driver";
                try {
                    driverName = fs::read_symlink(driverLink).filename().string();
                } catch (...) { continue; }
                if (driverName != "i915" && driverName != "xe") continue;
            }

            GpuInfo info;
            info.available = true;
            info.vendor    = "Intel";
            info.driver    = driverName;

            info.name = readSysfsString(devPath + "/product_name");
            if (info.name.empty()) {
                info.name = readSysfsString(devPath + "/label");
            }
            if (info.name.empty()) {
                std::string devId = readSysfsString(devPath + "/device");
                if (!devId.empty())
                    info.name = "Intel GPU (" + devId + ")";
                else
                    info.name = "Intel Integrated GPU";
            }

            {
                int64_t mhz = readSysfsInt(cardPath + "/gt_cur_freq_mhz");
                if (mhz > 0)
                    info.clockMHz = static_cast<float>(mhz);
            }

            std::string hwmon = findHwmonDir(devPath);
            if (!hwmon.empty()) {
                int64_t millideg = readSysfsInt(hwmon + "/temp1_input");
                if (millideg > 0)
                    info.temperature = static_cast<float>(millideg) / 1000.0f;

                int64_t uw = readSysfsInt(hwmon + "/power1_average");
                if (uw <= 0)
                    uw = readSysfsInt(hwmon + "/energy1_input");
                if (uw > 0)
                    info.powerWatts = static_cast<float>(uw) / 1000000.0f;
            }

            {
                int64_t total = readSysfsInt(devPath + "/mem_info_vram_total");
                int64_t used  = readSysfsInt(devPath + "/mem_info_vram_used");
                if (total > 0) {
                    info.memoryTotal = static_cast<uint64_t>(total);
                    info.memoryUsed  = (used > 0) ? static_cast<uint64_t>(used) : 0;
                    if (info.memoryTotal > 0) {
                        info.memoryPercent = static_cast<float>(info.memoryUsed) * 100.0f
                                           / static_cast<float>(info.memoryTotal);
                    }
                }
            }

            out.push_back(std::move(info));
        }
    } catch (...) {
    }
}

void LinuxGPU::update() {
    GpuSnapshot snap;

    if (nvmlSupported_) {
        queryNvml(snap.gpus);
    }

    queryAmdgpu(snap.gpus);

    queryIntel(snap.gpus);

    snap.supported = !snap.gpus.empty();

    std::lock_guard<std::mutex> lock(mutex_);
    current_ = std::move(snap);
}

GpuSnapshot LinuxGPU::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

#endif // __linux__
