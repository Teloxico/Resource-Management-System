/**
 * @file cpu_windows.cpp
 * @brief Windows CPU monitoring using GetSystemTimes, PDH, WMI, and Toolhelp.
 */

#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "cpu_windows.h"

#include <algorithm>
#include <numeric>
#include <string>

WindowsCPU::WindowsCPU() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    logicalCores_ = static_cast<int>(si.dwNumberOfProcessors);

    {
        DWORD len = 0;
        GetLogicalProcessorInformation(nullptr, &len);
        if (len > 0) {
            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buf(
                len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            if (GetLogicalProcessorInformation(buf.data(), &len)) {
                int phys = 0;
                for (auto& info : buf) {
                    if (info.Relationship == RelationProcessorCore)
                        ++phys;
                }
                physicalCores_ = phys > 0 ? phys : logicalCores_;
            } else {
                physicalCores_ = (logicalCores_ > 1) ? logicalCores_ / 2 : 1;
            }
        } else {
            physicalCores_ = (logicalCores_ > 1) ? logicalCores_ / 2 : 1;
        }
    }

    FILETIME idle{}, kernel{}, user{};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        prevIdle_   = ftToU64(idle);
        prevKernel_ = ftToU64(kernel);
        prevUser_   = ftToU64(user);
    }

    if (PdhOpenQuery(nullptr, 0, &pdhQuery_) == ERROR_SUCCESS) {
        coreCounters_.resize(logicalCores_, nullptr);
        for (int i = 0; i < logicalCores_; ++i) {
            std::wstring path = L"\\Processor(" + std::to_wstring(i) + L")\\% Processor Time";
            PdhAddCounter(pdhQuery_, path.c_str(), 0, &coreCounters_[i]);
        }

        PdhAddCounter(pdhQuery_,
                      L"\\Processor Information(_Total)\\Processor Frequency",
                      0, &freqCounter_);

        PdhAddCounter(pdhQuery_,
                      L"\\System\\Context Switches/sec",
                      0, &ctxSwitchCounter_);

        PdhAddCounter(pdhQuery_,
                      L"\\Processor(_Total)\\Interrupts/sec",
                      0, &interruptCounter_);

        PdhCollectQueryData(pdhQuery_);
        firstCollect_ = true;
    }

    usageHistory_.reserve(kMaxHistory);
}

WindowsCPU::~WindowsCPU() {
    if (pdhQuery_) {
        PdhCloseQuery(pdhQuery_);
        pdhQuery_ = nullptr;
    }
}

ULONGLONG WindowsCPU::ftToU64(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

/**
 * @brief Query CPU temperature via WMI MSAcpi_ThermalZoneTemperature.
 * @return Temperature in Celsius, or -1 on failure. Requires admin on most systems.
 */
float WindowsCPU::queryTemperatureWMI() const {
    float result = -1.0f;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool needUninit = SUCCEEDED(hr);
    if (FAILED(hr) && hr != S_FALSE && hr != RPC_E_CHANGED_MODE)
        return result;

    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                              RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE,
                              nullptr, EOAC_NONE, nullptr);

    IWbemLocator*  pLocator  = nullptr;
    IWbemServices* pServices = nullptr;
    IEnumWbemClassObject* pEnum = nullptr;

    do {
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IWbemLocator, reinterpret_cast<void**>(&pLocator));
        if (FAILED(hr)) break;

        hr = pLocator->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr,
                                     nullptr, 0, nullptr, nullptr, &pServices);
        if (FAILED(hr)) break;

        hr = CoSetProxyBlanket(pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                               nullptr, RPC_C_AUTHN_LEVEL_CALL,
                               RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) break;

        hr = pServices->ExecQuery(
            _bstr_t(L"WQL"),
            _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnum);
        if (FAILED(hr)) break;

        IWbemClassObject* pObj = nullptr;
        ULONG returned = 0;
        if (pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vtTemp;
            VariantInit(&vtTemp);
            if (SUCCEEDED(pObj->Get(L"CurrentTemperature", 0, &vtTemp, nullptr, nullptr))) {
                float kelvinTenths = static_cast<float>(vtTemp.lVal);
                result = (kelvinTenths / 10.0f) - 273.15f;
            }
            VariantClear(&vtTemp);
            pObj->Release();
        }
    } while (false);

    if (pEnum)     pEnum->Release();
    if (pServices) pServices->Release();
    if (pLocator)  pLocator->Release();
    if (needUninit) CoUninitialize();

    return result;
}

void WindowsCPU::update() {
    CpuSnapshot snap;
    snap.physicalCores = physicalCores_;
    snap.logicalCores  = logicalCores_;

    FILETIME idle{}, kernel{}, user{};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        ULONGLONG curIdle   = ftToU64(idle);
        ULONGLONG curKernel = ftToU64(kernel);
        ULONGLONG curUser   = ftToU64(user);

        ULONGLONG dIdle   = curIdle   - prevIdle_;
        ULONGLONG dKernel = curKernel - prevKernel_;
        ULONGLONG dUser   = curUser   - prevUser_;

        ULONGLONG dSystem = dKernel + dUser;

        if (dSystem > 0) {
            snap.totalUsage    = static_cast<float>(dSystem - dIdle) * 100.0f
                                 / static_cast<float>(dSystem);
            snap.systemPercent = static_cast<float>(dKernel - dIdle) * 100.0f
                                 / static_cast<float>(dSystem);
            snap.userPercent   = static_cast<float>(dUser) * 100.0f
                                 / static_cast<float>(dSystem);
            snap.idlePercent   = static_cast<float>(dIdle) * 100.0f
                                 / static_cast<float>(dSystem);
        }

        prevIdle_   = curIdle;
        prevKernel_ = curKernel;
        prevUser_   = curUser;
    }

    snap.iowaitPercent = -1.0f;
    snap.loadAvg1      = -1.0f;
    snap.loadAvg5      = -1.0f;
    snap.loadAvg15     = -1.0f;

    if (pdhQuery_) {
        PDH_STATUS status = PdhCollectQueryData(pdhQuery_);
        if (status == ERROR_SUCCESS && !firstCollect_) {
            snap.cores.resize(logicalCores_);
            for (int i = 0; i < logicalCores_; ++i) {
                snap.cores[i].id = i;
                if (coreCounters_[i]) {
                    PDH_FMT_COUNTERVALUE val{};
                    if (PdhGetFormattedCounterValue(coreCounters_[i],
                                                    PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
                        snap.cores[i].usage = static_cast<float>(val.doubleValue);
                    }
                }
                snap.cores[i].temperature = -1.0f;
            }

            if (freqCounter_) {
                PDH_FMT_COUNTERVALUE val{};
                if (PdhGetFormattedCounterValue(freqCounter_, PDH_FMT_DOUBLE,
                                                nullptr, &val) == ERROR_SUCCESS) {
                    snap.frequency = static_cast<float>(val.doubleValue);
                }
            }

            if (ctxSwitchCounter_) {
                PDH_FMT_COUNTERVALUE val{};
                if (PdhGetFormattedCounterValue(ctxSwitchCounter_, PDH_FMT_DOUBLE,
                                                nullptr, &val) == ERROR_SUCCESS) {
                    snap.contextSwitchesPerSec = static_cast<float>(val.doubleValue);
                }
            }

            if (interruptCounter_) {
                PDH_FMT_COUNTERVALUE val{};
                if (PdhGetFormattedCounterValue(interruptCounter_, PDH_FMT_DOUBLE,
                                                nullptr, &val) == ERROR_SUCCESS) {
                    snap.interruptsPerSec = static_cast<float>(val.doubleValue);
                }
            }
        }
        firstCollect_ = false;
    }

    if (snap.cores.empty()) {
        snap.cores.resize(logicalCores_);
        for (int i = 0; i < logicalCores_; ++i) {
            snap.cores[i].id = i;
            snap.cores[i].temperature = -1.0f;
        }
    }

    for (auto& c : snap.cores)
        c.frequency = snap.frequency;

    {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            DWORD myPid = GetCurrentProcessId();
            int totalThreads   = 0;
            int processThreads = 0;
            if (Thread32First(hSnap, &te)) {
                do {
                    ++totalThreads;
                    if (te.th32OwnerProcessID == myPid)
                        ++processThreads;
                } while (Thread32Next(hSnap, &te));
            }
            snap.totalThreads   = totalThreads;
            snap.processThreads = processThreads;
            CloseHandle(hSnap);
        }
    }

    snap.temperature = queryTemperatureWMI();

    {
        std::lock_guard<std::mutex> lock(mutex_);

        usageHistory_.push_back(snap.totalUsage);
        if (usageHistory_.size() > kMaxHistory)
            usageHistory_.erase(usageHistory_.begin());

        if (!usageHistory_.empty()) {
            float sum = std::accumulate(usageHistory_.begin(), usageHistory_.end(), 0.0f);
            snap.averageUsage = sum / static_cast<float>(usageHistory_.size());
            snap.highestUsage = *std::max_element(usageHistory_.begin(), usageHistory_.end());
        }

        current_ = std::move(snap);
    }
}

CpuSnapshot WindowsCPU::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

#endif // _WIN32
