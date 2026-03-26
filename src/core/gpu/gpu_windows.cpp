/**
 * @file gpu_windows.cpp
 * @brief Windows GPU monitoring implementation (DXGI + NVML).
 */

#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "gpu_windows.h"

#include <algorithm>
#include <cstring>
#include <string>

WindowsGPU::WindowsGPU() {
    enumerateDXGI();
    loadNvml();
}

WindowsGPU::~WindowsGPU() {
    unloadNvml();
}

void WindowsGPU::enumerateDXGI() {
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                    reinterpret_cast<void**>(&factory));
    if (FAILED(hr) || !factory) return;

    dxgiAdapters_.clear();

    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0;
         factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i)
    {
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) {
            adapter->Release();
            continue;
        }

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
        }

        DxgiAdapterEntry entry;

        char nameBuf[256]{};
        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
                            nameBuf, sizeof(nameBuf), nullptr, nullptr);
        entry.name     = nameBuf;
        entry.vendorId = desc.VendorId;
        entry.luid     = desc.AdapterLuid;
        entry.vramTotal = desc.DedicatedVideoMemory;

        switch (desc.VendorId) {
            case 0x10DE: entry.vendor = "NVIDIA"; break;
            case 0x1002: entry.vendor = "AMD";    break;
            case 0x8086: entry.vendor = "Intel";  break;
            default:     entry.vendor = "Unknown"; break;
        }

        dxgiAdapters_.push_back(std::move(entry));
        adapter->Release();
    }

    factory->Release();
    dxgiEnumerated_ = true;
}

void WindowsGPU::queryDXGIMemory(std::vector<GpuInfo>& out) {
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                    reinterpret_cast<void**>(&factory));
    if (FAILED(hr) || !factory) return;

    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0;
         factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i)
    {
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) {
            adapter->Release();
            continue;
        }

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
        }

        if (desc.VendorId == 0x10DE) {
            adapter->Release();
            continue;
        }

        DxgiAdapterEntry* cached = nullptr;
        for (auto& e : dxgiAdapters_) {
            if (e.luid.HighPart == desc.AdapterLuid.HighPart &&
                e.luid.LowPart  == desc.AdapterLuid.LowPart) {
                cached = &e;
                break;
            }
        }
        if (!cached) {
            adapter->Release();
            continue;
        }

        GpuInfo info;
        info.available    = true;
        info.name         = cached->name;
        info.vendor       = cached->vendor;
        info.memoryTotal  = cached->vramTotal;

        IDXGIAdapter3* adapter3 = nullptr;
        if (SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),
                                              reinterpret_cast<void**>(&adapter3)))) {
            DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
            if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(
                    0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
                info.memoryUsed = memInfo.CurrentUsage;
                if (info.memoryTotal > 0) {
                    info.memoryPercent =
                        static_cast<float>(info.memoryUsed) * 100.0f
                        / static_cast<float>(info.memoryTotal);
                }
            }
            adapter3->Release();
        }

        info.temperature = -1.0f;
        info.powerWatts  = -1.0f;
        info.fanPercent  = -1.0f;

        out.push_back(std::move(info));
        adapter->Release();
    }

    factory->Release();
}

void WindowsGPU::loadNvml() {
    nvmlLib_ = LoadLibraryA("nvml.dll");
    if (!nvmlLib_) {
        nvmlLib_ = LoadLibraryA("C:\\Windows\\System32\\nvml.dll");
    }
    if (!nvmlLib_) {
        nvmlSupported_ = false;
        return;
    }

    auto resolve = [this](const char* name) -> void* {
        return reinterpret_cast<void*>(GetProcAddress(nvmlLib_, name));
    };

    fnInit_       = reinterpret_cast<pfn_nvmlInit_v2>(resolve("nvmlInit_v2"));
    fnShutdown_   = reinterpret_cast<pfn_nvmlShutdown>(resolve("nvmlShutdown"));
    fnGetCount_   = reinterpret_cast<pfn_nvmlDeviceGetCount_v2>(resolve("nvmlDeviceGetCount_v2"));
    fnGetHandle_  = reinterpret_cast<pfn_nvmlDeviceGetHandleByIndex_v2>(resolve("nvmlDeviceGetHandleByIndex_v2"));
    fnGetName_    = reinterpret_cast<pfn_nvmlDeviceGetName>(resolve("nvmlDeviceGetName"));
    fnGetTemp_    = reinterpret_cast<pfn_nvmlDeviceGetTemperature>(resolve("nvmlDeviceGetTemperature"));
    fnGetUtil_    = reinterpret_cast<pfn_nvmlDeviceGetUtilizationRates>(resolve("nvmlDeviceGetUtilizationRates"));
    fnGetMemInfo_ = reinterpret_cast<pfn_nvmlDeviceGetMemoryInfo>(resolve("nvmlDeviceGetMemoryInfo"));
    fnGetPower_   = reinterpret_cast<pfn_nvmlDeviceGetPowerUsage>(resolve("nvmlDeviceGetPowerUsage"));
    fnGetFan_     = reinterpret_cast<pfn_nvmlDeviceGetFanSpeed>(resolve("nvmlDeviceGetFanSpeed"));
    fnGetClock_   = reinterpret_cast<pfn_nvmlDeviceGetClockInfo>(resolve("nvmlDeviceGetClockInfo"));
    fnGetDriver_  = reinterpret_cast<pfn_nvmlDeviceGetDriverVersion>(resolve("nvmlSystemGetDriverVersion"));

    if (!fnInit_ || !fnShutdown_ || !fnGetCount_ || !fnGetHandle_) {
        FreeLibrary(nvmlLib_);
        nvmlLib_ = nullptr;
        nvmlSupported_ = false;
        return;
    }

    if (fnInit_() != NVML_SUCCESS) {
        FreeLibrary(nvmlLib_);
        nvmlLib_ = nullptr;
        nvmlSupported_ = false;
        return;
    }

    unsigned int count = 0;
    if (fnGetCount_ && fnGetCount_(&count) == NVML_SUCCESS) {
        nvmlDeviceCount_ = count;
    }

    nvmlSupported_ = (nvmlDeviceCount_ > 0);
}

void WindowsGPU::unloadNvml() {
    if (fnShutdown_) fnShutdown_();
    if (nvmlLib_) { FreeLibrary(nvmlLib_); nvmlLib_ = nullptr; }
    nvmlSupported_ = false;
}

void WindowsGPU::queryNvml(std::vector<GpuInfo>& out) {
    std::string driverStr;
    if (fnGetDriver_) {
        char buf[128]{};
        if (fnGetDriver_(buf, sizeof(buf)) == NVML_SUCCESS)
            driverStr = buf;
    }

    for (unsigned int i = 0; i < nvmlDeviceCount_; ++i) {
        nvmlDevice_t dev = nullptr;
        if (!fnGetHandle_ || fnGetHandle_(i, &dev) != NVML_SUCCESS)
            continue;

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
                if (mem.total > 0) {
                    info.memoryPercent = static_cast<float>(mem.used) * 100.0f
                                       / static_cast<float>(mem.total);
                }
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

void WindowsGPU::update() {
    GpuSnapshot snap;

    if (!dxgiEnumerated_) {
        enumerateDXGI();
    }

    if (nvmlSupported_) {
        queryNvml(snap.gpus);
    }

    queryDXGIMemory(snap.gpus);

    if (!nvmlSupported_) {
        for (const auto& entry : dxgiAdapters_) {
            if (entry.vendor != "NVIDIA") continue;

            bool found = false;
            for (const auto& g : snap.gpus) {
                if (g.name == entry.name) { found = true; break; }
            }
            if (found) continue;

            GpuInfo info;
            info.available   = true;
            info.name        = entry.name;
            info.vendor      = "NVIDIA";
            info.memoryTotal = entry.vramTotal;
            info.temperature = -1.0f;
            info.powerWatts  = -1.0f;
            info.fanPercent  = -1.0f;
            snap.gpus.push_back(std::move(info));
        }
    }

    snap.supported = !snap.gpus.empty();

    std::lock_guard<std::mutex> lock(mutex_);
    current_ = std::move(snap);
}

GpuSnapshot WindowsGPU::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

#endif // _WIN32
