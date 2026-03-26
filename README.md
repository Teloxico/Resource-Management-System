# ResourceMonitor

A cross-platform system monitoring tool that tracks CPU, memory, network, disk, GPU, and process activity in real time. Includes a terminal interface (CLI) and a graphical dashboard (GUI) built on Dear ImGui and ImPlot. Metrics are saved to a local SQLite database so you can query history and export data.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Building from Source](#building-from-source)
  - [Windows](#windows)
  - [Linux](#linux)
- [Running](#running)
- [Project Layout](#project-layout)
- [How It Works](#how-it-works)
  - [Architecture Overview](#architecture-overview)
  - [CPU Monitoring](#cpu-monitoring)
  - [Memory Monitoring](#memory-monitoring)
  - [Network Monitoring](#network-monitoring)
  - [Disk Monitoring](#disk-monitoring)
  - [GPU Monitoring](#gpu-monitoring)
  - [Process Manager](#process-manager)
  - [Alert Engine](#alert-engine)
  - [Database & Export](#database--export)
  - [Logger](#logger)
- [Generating API Documentation (Doxygen)](#generating-api-documentation-doxygen)
- [Running the Tests](#running-the-tests)
- [License](#license)

---

## Features

- **CPU** -- total and per-core usage, frequency, temperature (where the hardware exposes it), context switches, interrupts, load averages (Linux).
- **Memory** -- physical and swap usage, cached/buffered breakdown, page fault rate, top-5 memory consumers.
- **Network** -- per-interface upload/download rates, IP and MAC addresses, full TCP and UDP connection table with owning process names.
- **Disk** -- per-volume space usage, read/write throughput in bytes/sec and IOPS, disk utilisation percentage.
- **GPU** -- NVIDIA (via NVML), AMD (amdgpu sysfs), Intel (i915/xe sysfs), plus DXGI VRAM queries on Windows. Reports utilisation, VRAM, temperature, power draw, fan speed, and clock frequencies where the driver provides them.
- **Processes** -- full process list with CPU%, memory, disk I/O rates, threads, user, command line. You can kill a process or change its priority straight from the GUI.
- **Alerts** -- threshold-based rules that fire when a metric exceeds (or drops below) a value for a sustained period. Alerts are logged to the database and shown in the GUI.
- **Database** -- SQLite with WAL journaling. Metrics are recorded every few seconds. You can prune old data, or export to CSV/TXT filtered by timeframe and metric category.
- **CLI** -- a lightweight terminal dashboard that refreshes once per second.
- **GUI** -- a tabbed Dear ImGui interface with real-time ImPlot charts, a process manager, alert configuration, system info, and data export controls.

---

## Prerequisites

Install these before building:

| Tool | Minimum Version | Download |
|------|----------------|----------|
| **CMake** | 3.15+ | [cmake.org/download](https://cmake.org/download/) |
| **C++ compiler** with C++17 support | GCC 8+, Clang 7+, or MSVC 2019+ | [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) (Windows) |
| **Git** | any recent version | [git-scm.com](https://git-scm.com/downloads) |
| **OpenGL drivers** | 3.3+ (for the GUI only) | Usually already installed with your GPU driver |

Optional (for generating API docs):

| Tool | Download |
|------|----------|
| **Doxygen** | [doxygen.nl/download](https://www.doxygen.nl/download.html) |
| **Graphviz** (for call/dependency graphs) | [graphviz.org/download](https://graphviz.org/download/) |

You do **not** need to install GLFW, Dear ImGui, ImPlot, SQLite, or Google Test yourself -- CMake's `FetchContent` pulls and compiles them automatically during the first build.

---

## Building from Source

### Windows

1. **Open a Developer Command Prompt** (or any terminal where `cl` and `cmake` are on your PATH). If you have Visual Studio installed, search the Start menu for "Developer Command Prompt" or "Developer PowerShell".

2. **Clone the repository:**
   ```powershell
   git clone https://github.com/Teloxico/Resource-Management-System.git
   cd ResourceMonitor
   ```

3. **Configure the build:**
   ```powershell
   mkdir build
   cd build
   cmake .. -DBUILD_GUI=ON -DBUILD_CLI=ON -DBUILD_TESTS=ON
   ```

4. **Compile:**
   ```powershell
   cmake --build . --config Release
   ```
   The first build takes a few minutes while CMake fetches and compiles GLFW, ImGui, ImPlot, SQLite, and Google Test. Incremental builds only recompile changed files.

5. **Find your binaries** in `build\Release\`:
   - `ResourceMonitorCLI.exe`
   - `ResourceMonitorGUI.exe`

> **Tip:** If you only want the CLI (no GPU/OpenGL dependency), pass `-DBUILD_GUI=OFF` at step 3.

### Linux

1. **Install build dependencies** (Debian / Ubuntu):
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake git \
       libgl-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
   ```
   The `libgl` and `libx*` packages are needed by GLFW for windowing. If you are skipping the GUI (`-DBUILD_GUI=OFF`), you only need `build-essential`, `cmake`, and `git`.

2. **Clone and build:**
   ```bash
   git clone https://github.com/Teloxico/Resource-Management-System.git
   cd ResourceMonitor
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)
   ```

3. **Binaries are in `build/`:**
   - `ResourceMonitorCLI`
   - `ResourceMonitorGUI`

---

## Running

### CLI

```bash
# Windows
build\Release\ResourceMonitorCLI.exe

# Linux
./build/ResourceMonitorCLI
```

The CLI clears the terminal each second and prints a formatted table of CPU, memory, network, disk, and GPU metrics. Every 10 ticks it writes a snapshot to `resource_monitor.db`. Press **Ctrl+C** to stop -- on exit it exports all collected data to CSV files in the current directory.

> **Note on privileges:** Some metrics need elevated access. On Windows, CPU temperature via WMI requires running as Administrator. On Linux, per-process disk I/O (`/proc/[pid]/io`) and socket-to-PID mapping (`/proc/[pid]/fd/`) require root or `CAP_SYS_PTRACE`. The monitor still works without elevation -- those fields just show as unavailable.

### GUI

```bash
# Windows
build\Release\ResourceMonitorGUI.exe

# Linux
./build/ResourceMonitorGUI
```

The GUI opens a window (sized to 60% of your screen) with tabs for Overview, CPU, Memory, Network, Disk, GPU, Processes, Alerts, and System Info. A background thread samples metrics at roughly one-second intervals while the UI renders at your monitor's vsync rate.

---

## Project Layout

```
ResourceMonitor/
|-- CMakeLists.txt              Root build script (FetchContent for all dependencies)
|-- Doxyfile                    Doxygen configuration
|-- setup.sh                    Linux quick-setup helper
|-- setup.ps1                   Windows quick-setup helper
|-- src/
|   |-- CMakeLists.txt          Routes to sub-modules
|   |-- core/
|   |   |-- metrics.h           All snapshot structs (CpuSnapshot, MemorySnapshot, etc.)
|   |   |-- cpu/                CPU monitor: abstract base + Windows + Linux + factory
|   |   |-- memory/             Memory monitor: same pattern
|   |   |-- network/            Network monitor: same pattern
|   |   |-- disk/               Disk monitor: same pattern
|   |   |-- gpu/                GPU monitor: NVML, DXGI, amdgpu sysfs, i915 sysfs
|   |   |-- process/            Process manager: enumerate, kill, reprioritise
|   |   |-- system_info/        Static system info (OS, CPU model, cache sizes, uptime)
|   |   |-- alerts/             Threshold-based alert engine
|   |   |-- database/           SQLite persistence and CSV/TXT export
|   |-- cli/
|   |   |-- main.cpp            CLI entry point and display loop
|   |   |-- cli_interface.h     (Placeholder for future CLI commands)
|   |-- gui/
|   |   |-- main.cpp            GLFW/ImGui initialisation and main loop
|   |   |-- app.h               App class: collector thread, render methods, history buffers
|   |   |-- theme.h             Dark colour scheme, severity palette, card helpers
|   |-- utils/
|   |   |-- logger.h/.cpp       Thread-safe file+console logger with severity levels
|   |   |-- scrolling_buffer.h  Ring buffer for real-time ImPlot charts
|   |-- tests/                  Google Test suites for each module
```

---

## How It Works

### Architecture Overview

Every hardware subsystem follows the same pattern:

1. An **abstract base class** (`CPU`, `Memory`, `Network`, `Disk`, `GPU`, `ProcessManager`) declares `update()` and `snapshot()`.
2. A **platform implementation** (`WindowsCPU`, `LinuxCPU`, etc.) collects data from OS-specific APIs.
3. A **factory function** (`createCPU()`, `createMemory()`, ...) returns the right implementation at compile time via `#ifdef _WIN32` / `__linux__`.

A background **collector thread** calls `update()` on every module once per second, then stores the combined `MetricData` snapshot under a mutex. The render loop (GUI) or display loop (CLI) reads that snapshot whenever it needs to draw.

Thread safety is handled per-module: each implementation guards its internal state with a `std::mutex` so that `update()` and `snapshot()` can run on different threads without races.

### CPU Monitoring

**Windows:** `GetSystemTimes()` gives overall user/kernel/idle tick deltas. PDH counters (`\Processor(N)\% Processor Time`) track per-core usage. Processor frequency comes from `\Processor Information(_Total)\Processor Frequency`. Temperature is read through WMI's `MSAcpi_ThermalZoneTemperature` (needs admin on most machines). Thread counts come from `CreateToolhelp32Snapshot` iterating the thread list.

**Linux:** Parses `/proc/stat` for aggregate and per-core tick deltas (user, nice, system, idle, iowait, irq, softirq, steal). Frequency is read from sysfs (`scaling_cur_freq`) with a fallback to `/proc/cpuinfo`. Temperature comes from `/sys/class/hwmon`, trying known sensor drivers (coretemp, k10temp, zenpower, etc.) first. Load averages are from `/proc/loadavg`.

Both platforms keep a rolling 300-sample history to compute running averages and peak values.

### Memory Monitoring

**Windows:** `GlobalMemoryStatusEx` for total/available/used physical memory and swap. `GetPerformanceInfo` for committed memory and kernel pool sizes. PDH counters for page faults/sec and cache bytes. Top-5 processes by working set are found via `EnumProcesses` + `GetProcessMemoryInfo`, refreshed every 5 seconds to avoid overhead.

**Linux:** Parses `/proc/meminfo` for MemTotal, MemAvailable, Buffers, Cached, SReclaimable, Swap*, Committed_AS, and CommitLimit. Page faults come from `/proc/vmstat` (`pgfault` delta over elapsed time). Top processes are found by iterating `/proc/[pid]/status` for VmRSS.

### Network Monitoring

**Windows:** `GetIfTable2` for per-interface byte/packet/error/drop counters. `GetAdaptersAddresses` for IP and MAC addresses. `GetExtendedTcpTable` and `GetExtendedUdpTable` (both IPv4 and IPv6) for the full connection table with owning PIDs. Process names are resolved via `GetModuleBaseNameA` and cached per-PID.

**Linux:** Parses `/proc/net/dev` for interface counters, `getifaddrs()` for IP and MAC addresses, sysfs for link speed and operstate. TCP connections come from `/proc/net/tcp` and `/proc/net/tcp6`; UDP from `/proc/net/udp` and `/proc/net/udp6`. Socket-to-PID mapping is built by scanning `/proc/[pid]/fd/` for `socket:[inode]` symlinks, refreshed every 5 seconds.

Upload and download rates are computed as byte-count deltas divided by elapsed wall-clock time.

### Disk Monitoring

**Windows:** `GetLogicalDriveStrings` + `GetDiskFreeSpaceEx` for per-volume capacity. PDH `PhysicalDisk` counters for read/write bytes/sec, IOPS, and % disk time. Per-physical-disk counters are enumerated with `PdhEnumObjectItems` and matched back to logical drive letters.

**Linux:** Parses `/proc/mounts` for real block devices (filtering out virtual filesystems), `statvfs()` for capacity, and `/proc/diskstats` for I/O counters. Throughput is sector-delta-based (512 bytes per sector), computed over the elapsed interval. Whole-disk devices (sda, nvme0n1) are distinguished from partition names for aggregate rate totals.

### GPU Monitoring

**NVIDIA:** Loads `nvml.dll` (Windows) or `libnvidia-ml.so` (Linux) at runtime via `LoadLibrary`/`dlopen`. Queries utilisation, VRAM, temperature, power draw, fan speed, and core/memory clock frequencies through the NVML C API. If the library is missing or initialisation fails, the GPU tab stays empty instead of crashing.

**AMD (Linux only):** Reads amdgpu sysfs files under `/sys/class/drm/cardN/device/` -- `gpu_busy_percent`, `mem_info_vram_total`, `mem_info_vram_used`, hwmon for temperature/power/fan, and `pp_dpm_sclk`/`pp_dpm_mclk` for active clock frequencies.

**Intel (Linux only):** Similar sysfs approach for the i915 or xe driver -- `gt_cur_freq_mhz` for clock speed, hwmon for thermals and power.

**Windows (non-NVIDIA):** Uses DXGI `IDXGIAdapter3::QueryVideoMemoryInfo` to report VRAM usage on AMD and Intel GPUs. Detailed utilisation/temperature data is not available through DXGI alone.

### Process Manager

**Windows:** `CreateToolhelp32Snapshot` for the process list. `GetProcessTimes` for CPU tick deltas (normalised by wall-clock time and processor count). `GetProcessMemoryInfo` for the working set. `QueryFullProcessImageNameA` for the executable path. `OpenProcessToken` + `LookupAccountSidA` for the owning user name. `GetProcessIoCounters` for cumulative read/write bytes (rates from deltas). Kill via `TerminateProcess`, priority change via `SetPriorityClass`.

**Linux:** Iterates `/proc/[pid]/` directories. Reads `stat` for process state, parent PID, priority, nice value, thread count, and CPU tick deltas (utime + stime). Reads `status` for VmRSS and UID. Reads `cmdline` for the full command line (null bytes replaced with spaces). Reads `io` for disk byte counters. Kill via `SIGTERM`, priority change via `setpriority()`.

### Alert Engine

`AlertManager` holds a list of `AlertRule` objects. Each rule specifies a metric, a threshold, a direction (above or below), and a sustained duration in seconds. Supported metrics include CPU usage, memory, swap, disk, GPU, temperatures, and network rates.

On every tick, `evaluate()` loops through enabled rules:
- If the condition is met, the sustained counter goes up.
- Once that counter reaches the required duration and the rule hasn't already triggered, it creates an `AlertEvent` with a timestamp and human-readable message, stores it, and calls the registered callback (if any).
- If the condition stops being met, the counter and triggered flag reset so the rule can fire again later.

The event log is capped at 1,000 entries. All public methods are mutex-guarded so they can be called from any thread.

### Database & Export

`Database` opens a SQLite file in WAL mode for concurrent read performance. Six tables store timestamped data: `cpu_metrics`, `memory_metrics`, `network_metrics`, `disk_metrics`, `gpu_metrics`, and `alert_events`. Each has a timestamp index for fast range queries.

Inserts use prepared statements, batched in a single transaction per snapshot so writes don't bottleneck.

`pruneOlderThan(days)` bulk-deletes rows with timestamps older than the cutoff. `exportToCSV()` dumps all tables. `exportFiltered()` lets you choose which tables, a time window (last N hours), and whether the output should be comma-separated (`.csv`) or tab-separated (`.txt`).

### Logger

A static, thread-safe logger with four severity levels: Debug, Info, Warning, Error. Each log line includes a millisecond-precision timestamp and a severity tag. Output always goes to a log file; console output is optional. Warnings and errors are sent to `stderr`, everything else to `stdout`.

---

## Generating API Documentation (Doxygen)

A `Doxyfile` is included in the repo root. To generate the HTML docs:

```bash
cd ResourceMonitor
doxygen Doxyfile
```

The output lands in `docs/html/`. Open `docs/html/index.html` in your browser to explore class hierarchies, call graphs (if Graphviz is installed), and annotated source listings.

---

## Running the Tests

Tests are built by default (`BUILD_TESTS=ON`). After building:

```bash
# Windows
cd build
ctest --output-on-failure -C Release

# Linux
cd build
ctest --output-on-failure
```

You can also run individual test executables directly from the build directory.

---

## License

This project does not currently include a licence file. Add a `LICENSE` if you plan to distribute it publicly.
