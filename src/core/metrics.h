/**
 * @file metrics.h
 * @brief Data structures for resource monitoring snapshots.
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

/// @brief Per-core CPU information.
struct CoreInfo {
    int    id            = 0;       ///< Core index.
    float  usage         = 0.0f;    ///< Usage percentage (0-100).
    float  frequency     = 0.0f;    ///< Clock speed in MHz.
    float  temperature   = -1.0f;   ///< Temperature in Celsius, -1 if unavailable.
};

/// @brief Aggregated and per-core CPU metrics.
struct CpuSnapshot {
    float  totalUsage    = 0.0f;    ///< Overall CPU usage percentage (0-100).
    float  userPercent   = 0.0f;    ///< Time spent in user mode.
    float  systemPercent = 0.0f;    ///< Time spent in kernel mode.
    float  idlePercent   = 0.0f;    ///< Idle time percentage.
    float  iowaitPercent = -1.0f;   ///< I/O wait percentage (Linux only, -1 on Windows).

    float  frequency     = 0.0f;    ///< Average frequency across cores in MHz.

    int    physicalCores = 0;       ///< Number of physical cores.
    int    logicalCores  = 0;       ///< Number of logical cores.
    int    totalThreads  = 0;       ///< System-wide thread count.
    int    processThreads= 0;       ///< Thread count for the current process.

    float  loadAvg1      = -1.0f;   ///< 1-minute load average (Linux only, -1 on Windows).
    float  loadAvg5      = -1.0f;   ///< 5-minute load average.
    float  loadAvg15     = -1.0f;   ///< 15-minute load average.

    float  temperature   = -1.0f;   ///< Package temperature in Celsius, -1 if unavailable.

    float  contextSwitchesPerSec = 0.0f; ///< Context switches per second.
    float  interruptsPerSec      = 0.0f; ///< Hardware interrupts per second.

    std::vector<CoreInfo> cores;    ///< Per-core details.

    float  averageUsage  = 0.0f;    ///< Running average of total usage.
    float  highestUsage  = 0.0f;    ///< Peak usage observed.
};

/// @brief Physical and virtual memory metrics.
struct MemorySnapshot {
    uint64_t totalBytes      = 0;   ///< Total physical memory in bytes.
    uint64_t usedBytes       = 0;   ///< Used physical memory in bytes.
    uint64_t availableBytes  = 0;   ///< Available physical memory in bytes.
    uint64_t cachedBytes     = 0;   ///< Linux page cache in bytes.
    uint64_t bufferedBytes   = 0;   ///< Linux buffers in bytes.
    float    usagePercent    = 0.0f; ///< Memory usage percentage.

    uint64_t swapTotal       = 0;   ///< Total swap space in bytes.
    uint64_t swapUsed        = 0;   ///< Used swap space in bytes.
    uint64_t swapFree        = 0;   ///< Free swap space in bytes.
    float    swapPercent     = 0.0f; ///< Swap usage percentage.

    uint64_t committedBytes  = 0;   ///< Committed memory in bytes.
    uint64_t commitLimitBytes= 0;   ///< Commit limit in bytes.

    float    pageFaultsPerSec= 0.0f; ///< Page faults per second.

    std::string topProcessName;      ///< Name of the top memory-consuming process.
    uint64_t    topProcessMemory = 0;///< Memory used by the top process in bytes.

    uint64_t pagedPoolBytes = 0;     ///< Windows paged pool size in bytes.
    uint64_t nonPagedPoolBytes = 0;  ///< Windows non-paged pool size in bytes.

    /// @brief A process entry for the top-consumers list.
    struct TopProcess {
        std::string name;            ///< Process name.
        uint64_t memoryBytes = 0;    ///< Memory used in bytes.
    };
    std::vector<TopProcess> topProcesses; ///< Top 5 memory consumers.

    float    averageUsage    = 0.0f; ///< Running average of memory usage.
};

/// @brief Per-interface network statistics.
struct NetworkInterfaceInfo {
    std::string name;                ///< Interface name.
    std::string ipAddress;           ///< IP address.
    std::string macAddress;          ///< MAC address.
    bool     isUp           = false; ///< Whether the interface is active.
    float    linkSpeedMbps  = 0.0f;  ///< Link speed in Mbps.
    float    uploadRate     = 0.0f;  ///< Upload rate in bytes/sec.
    float    downloadRate   = 0.0f;  ///< Download rate in bytes/sec.
    uint64_t totalSent      = 0;     ///< Total bytes sent.
    uint64_t totalRecv      = 0;     ///< Total bytes received.
    uint64_t packetsIn      = 0;     ///< Inbound packet count.
    uint64_t packetsOut     = 0;     ///< Outbound packet count.
    uint64_t errorsIn       = 0;     ///< Inbound error count.
    uint64_t errorsOut      = 0;     ///< Outbound error count.
    uint64_t dropsIn        = 0;     ///< Inbound drop count.
    uint64_t dropsOut       = 0;     ///< Outbound drop count.
};

/// @brief A single TCP connection entry.
struct TcpConnection {
    std::string localAddr;           ///< Local address.
    uint16_t    localPort    = 0;    ///< Local port number.
    std::string remoteAddr;          ///< Remote address.
    uint16_t    remotePort   = 0;    ///< Remote port number.
    std::string state;               ///< TCP state (e.g. ESTABLISHED).
    int         pid          = 0;    ///< Owning process ID.
    std::string processName;         ///< Owning process name.
};

/// @brief Aggregated network metrics across all interfaces.
struct NetworkSnapshot {
    float    totalUploadRate   = 0.0f;   ///< Aggregate upload rate in bytes/sec.
    float    totalDownloadRate = 0.0f;   ///< Aggregate download rate in bytes/sec.
    uint64_t totalBytesSent   = 0;       ///< Total bytes sent across all interfaces.
    uint64_t totalBytesRecv   = 0;       ///< Total bytes received across all interfaces.
    float    highestUpload    = 0.0f;    ///< Peak upload rate observed.
    float    highestDownload  = 0.0f;    ///< Peak download rate observed.
    std::string topProcess;              ///< Process with highest network activity.
    std::vector<NetworkInterfaceInfo> interfaces; ///< Per-interface details.
    std::vector<TcpConnection>        connections;///< Active TCP connections.
};

/// @brief Per-disk storage and I/O metrics.
struct DiskInfo {
    std::string device;              ///< Device path (e.g. "/dev/sda", "C:").
    std::string mountPoint;          ///< Mount point (e.g. "/", "C:\\").
    std::string fsType;              ///< Filesystem type (e.g. "ext4", "NTFS").
    uint64_t totalBytes      = 0;    ///< Total capacity in bytes.
    uint64_t usedBytes       = 0;    ///< Used space in bytes.
    uint64_t freeBytes       = 0;    ///< Free space in bytes.
    float    usagePercent    = 0.0f; ///< Usage percentage.
    float    readBytesPerSec = 0.0f; ///< Read throughput in bytes/sec.
    float    writeBytesPerSec= 0.0f; ///< Write throughput in bytes/sec.
    float    readOpsPerSec   = 0.0f; ///< Read operations per second.
    float    writeOpsPerSec  = 0.0f; ///< Write operations per second.
    float    utilizationPct  = 0.0f; ///< Disk busy percentage.
    float    temperature     = -1.0f;///< SMART temperature in Celsius, -1 if unavailable.
};

/// @brief Aggregated disk snapshot across all volumes.
struct DiskSnapshot {
    std::vector<DiskInfo> disks;     ///< Per-disk details.
    float totalReadRate  = 0.0f;     ///< Aggregate read rate in bytes/sec.
    float totalWriteRate = 0.0f;     ///< Aggregate write rate in bytes/sec.
};

/// @brief Per-GPU status and telemetry.
struct GpuInfo {
    std::string name;                ///< GPU model name.
    std::string vendor;              ///< Vendor string ("NVIDIA", "AMD", "Intel", "Unknown").
    std::string driver;              ///< Driver version string.
    float    utilization   = 0.0f;   ///< GPU utilization percentage (0-100).
    uint64_t memoryUsed    = 0;      ///< Used VRAM in bytes.
    uint64_t memoryTotal   = 0;      ///< Total VRAM in bytes.
    float    memoryPercent = 0.0f;   ///< VRAM usage percentage.
    float    temperature   = -1.0f;  ///< Temperature in Celsius, -1 if unavailable.
    float    powerWatts    = -1.0f;  ///< Power draw in watts, -1 if unavailable.
    float    fanPercent    = -1.0f;  ///< Fan speed percentage, -1 if unavailable.
    float    clockMHz      = 0.0f;   ///< Core clock in MHz.
    float    memClockMHz   = 0.0f;   ///< Memory clock in MHz.
    bool     available     = false;  ///< Whether the GPU is accessible.
};

/// @brief Snapshot of all detected GPUs.
struct GpuSnapshot {
    std::vector<GpuInfo> gpus;       ///< Per-GPU details.
    bool supported = false;          ///< Whether GPU monitoring is supported.
};

/// @brief Information about a single OS process.
struct ProcessInfo {
    int         pid           = 0;   ///< Process ID.
    int         ppid          = 0;   ///< Parent process ID.
    std::string name;                ///< Process name.
    std::string path;                ///< Executable path.
    std::string cmdline;             ///< Full command line.
    std::string user;                ///< Owning user.
    char        state         = '?'; ///< Process state (R, S, D, Z, T, etc.).
    float       cpuPercent    = 0.0f;///< CPU usage percentage.
    uint64_t    memoryBytes   = 0;   ///< Resident memory in bytes.
    float       memoryPercent = 0.0f;///< Memory usage percentage.
    int64_t     readBytesPerSec  = 0;///< Disk read rate in bytes/sec.
    int64_t     writeBytesPerSec = 0;///< Disk write rate in bytes/sec.
    int         threads       = 0;   ///< Thread count.
    int         priority      = 0;   ///< Scheduling priority.
    int         nice          = 0;   ///< Nice value.
};

/// @brief Snapshot of all running processes.
struct ProcessSnapshot {
    std::vector<ProcessInfo> processes; ///< List of process entries.
    int totalProcesses   = 0;          ///< Total process count.
    int totalThreads     = 0;          ///< Total thread count system-wide.
    int runningProcesses = 0;          ///< Number of processes in running state.
};

/// @brief Static system information, typically queried once at startup.
struct SystemInfoSnapshot {
    std::string osName;              ///< Operating system name.
    std::string osVersion;           ///< OS version string.
    std::string kernelVersion;       ///< Kernel version string.
    std::string hostname;            ///< Machine hostname.
    std::string arch;                ///< CPU architecture (e.g. "x86_64").
    std::string cpuModel;            ///< CPU model name.
    int         cpuPhysicalCores = 0;///< Physical core count.
    int         cpuLogicalCores  = 0;///< Logical core count.
    uint32_t    l1CacheKB        = 0;///< L1 cache size in KB.
    uint32_t    l2CacheKB        = 0;///< L2 cache size in KB.
    uint32_t    l3CacheKB        = 0;///< L3 cache size in KB.
    uint64_t    totalRAM         = 0;///< Total physical RAM in bytes.
    std::string gpuModel;            ///< Primary GPU model name.
    uint64_t    uptimeSeconds    = 0;///< System uptime in seconds.
};

/// @brief Metric categories that alert rules can monitor.
enum class AlertMetric {
    CpuUsage, MemoryUsage, SwapUsage, DiskUsage,
    GpuUsage, CpuTemp, GpuTemp, NetUpload, NetDownload
};

/// @brief A threshold-based alert rule for a specific metric.
struct AlertRule {
    int          id              = 0;       ///< Unique rule identifier.
    std::string  name;                      ///< Human-readable rule name.
    AlertMetric  metric          = AlertMetric::CpuUsage; ///< Metric to watch.
    float        threshold       = 90.0f;   ///< Threshold value.
    bool         above           = true;    ///< True = trigger when value exceeds threshold.
    int          sustainSeconds  = 5;       ///< Seconds the condition must hold before firing.
    bool         enabled         = true;    ///< Whether the rule is active.
    bool         triggered       = false;   ///< Runtime: currently in triggered state.
    float        currentValue    = 0.0f;    ///< Runtime: last evaluated metric value.
    int          sustainedCount  = 0;       ///< Runtime: consecutive ticks condition was met.
    std::string  lastTriggered;             ///< Runtime: timestamp of last trigger.
};

/// @brief A recorded alert event with context.
struct AlertEvent {
    std::string timestamp;           ///< When the alert fired (ISO 8601).
    std::string ruleName;            ///< Name of the rule that fired.
    std::string message;             ///< Human-readable alert message.
    float       value     = 0.0f;    ///< Metric value at the time of the alert.
    float       threshold = 0.0f;    ///< Threshold that was breached.
};

/// @brief Master snapshot filled by the collector thread each tick.
struct MetricData {
    CpuSnapshot        cpu;          ///< CPU metrics.
    MemorySnapshot     memory;       ///< Memory metrics.
    NetworkSnapshot    network;      ///< Network metrics.
    DiskSnapshot       disk;         ///< Disk metrics.
    GpuSnapshot        gpu;          ///< GPU metrics.
    ProcessSnapshot    process;      ///< Process metrics.
    SystemInfoSnapshot systemInfo;   ///< Static system information.
};
