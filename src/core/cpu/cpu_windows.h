/**
 * @file cpu_windows.h
 * @brief Windows implementation of the CPU monitoring interface.
 */

#pragma once

#ifdef _WIN32

#include "cpu_common.h"

#include <windows.h>
#include <pdh.h>
#include <tlhelp32.h>
#include <comdef.h>
#include <Wbemidl.h>

#include <vector>
#include <mutex>
#include <chrono>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")

/**
 * @brief Windows CPU monitor using GetSystemTimes, PDH, and WMI.
 */
class WindowsCPU : public CPU {
public:
    WindowsCPU();
    ~WindowsCPU() override;

    /**
     * @brief Collect CPU metrics from Windows APIs.
     */
    void        update()                  override;

    /**
     * @brief Get a thread-safe copy of the latest snapshot.
     * @return Most recent CpuSnapshot.
     */
    CpuSnapshot snapshot() const          override;

private:
    /**
     * @brief Convert a FILETIME to a 64-bit unsigned integer.
     * @param ft FILETIME to convert.
     * @return 64-bit tick count.
     */
    static ULONGLONG ftToU64(const FILETIME& ft);

    /**
     * @brief Query CPU temperature via WMI.
     * @return Temperature in Celsius, or -1 on failure.
     */
    float            queryTemperatureWMI() const;

    PDH_HQUERY   pdhQuery_          = nullptr; ///< PDH query handle
    PDH_HCOUNTER freqCounter_       = nullptr; ///< Processor frequency counter
    PDH_HCOUNTER ctxSwitchCounter_  = nullptr; ///< Context switches/sec counter
    PDH_HCOUNTER interruptCounter_  = nullptr; ///< Interrupts/sec counter
    std::vector<PDH_HCOUNTER> coreCounters_;   ///< Per-core usage counters

    ULONGLONG prevIdle_   = 0; ///< Previous idle ticks for delta calculation
    ULONGLONG prevKernel_ = 0; ///< Previous kernel ticks for delta calculation
    ULONGLONG prevUser_   = 0; ///< Previous user ticks for delta calculation

    int physicalCores_ = 0; ///< Physical core count
    int logicalCores_  = 0; ///< Logical core count (includes hyperthreads)

    static constexpr size_t kMaxHistory = 300; ///< Max stored usage samples
    std::vector<float> usageHistory_; ///< Rolling CPU usage history

    mutable std::mutex mutex_; ///< Guards current_
    CpuSnapshot        current_; ///< Latest snapshot

    bool firstCollect_ = true; ///< True until second PDH collect completes
};

#endif // _WIN32
