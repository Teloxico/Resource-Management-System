/**
 * @file main.cpp
 * @brief CLI resource monitor using the new snapshot-based API.
 *
 * Updates once per second, displays CPU, Memory, Network, Disk, and GPU
 * metrics in a formatted table.  Persists data to SQLite.
 */

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <string>
#include <cstdio>

#include "core/cpu/cpu_common.h"
#include "core/memory/memory_common.h"
#include "core/network/network_common.h"
#include "core/disk/disk_common.h"
#include "core/gpu/gpu_common.h"
#include "core/process/process_common.h"
#include "core/system_info/system_info.h"
#include "core/database/database.h"
#include "utils/logger.h"

static std::atomic<bool> running{true};

static void signalHandler(int) { running = false; }

static void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif
}

static std::string center(const std::string& s, int w) {
    int pad = (w - static_cast<int>(s.size())) / 2;
    if (pad <= 0) return s;
    return std::string(pad, ' ') + s;
}

static std::string fmtBytes(uint64_t b) {
    const char* u[] = {"B","KB","MB","GB","TB"};
    double v = static_cast<double>(b); int i = 0;
    while (v >= 1024 && i < 4) { v /= 1024; i++; }
    char buf[32]; snprintf(buf, 32, "%.1f %s", v, u[i]);
    return buf;
}

static std::string fmtRate(float b) {
    const char* u[] = {"B/s","KB/s","MB/s","GB/s"};
    double v = static_cast<double>(b); int i = 0;
    while (v >= 1024 && i < 3) { v /= 1024; i++; }
    char buf[32]; snprintf(buf, 32, "%.1f %s", v, u[i]);
    return buf;
}

int main() {
    Logger::initialize("resource_monitor.log");
    signal(SIGINT, signalHandler);

    auto cpu     = createCPU();
    auto memory  = createMemory();
    auto network = createNetwork();
    auto disk    = createDisk();
    auto gpu     = createGPU();
    SystemInfo sysInfo;
    Database db("resource_monitor.db");
    db.initialize();

    if (!cpu || !memory || !network) {
        std::cerr << "Failed to initialise monitoring modules.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Monitoring resources... (Ctrl+C to stop)\n";
    Logger::log("CLI started");

    int tick = 0;
    const int W = 90;

    while (running) {
        // Update all modules
        cpu->update();
        memory->update();
        network->update();
        if (disk) disk->update();
        if (gpu) gpu->update();
        sysInfo.update();

        // Grab snapshots
        auto cs = cpu->snapshot();
        auto ms = memory->snapshot();
        auto ns = network->snapshot();
        DiskSnapshot ds; if (disk) ds = disk->snapshot();
        GpuSnapshot gs;  if (gpu)  gs = gpu->snapshot();

        // Build MetricData for DB
        MetricData md;
        md.cpu = cs; md.memory = ms; md.network = ns; md.disk = ds; md.gpu = gs;
        md.systemInfo = sysInfo.snapshot();

        if (++tick % 10 == 0) db.insertSnapshot(md);

        clearConsole();

        auto line = [&](){ std::cout << std::string(W, '-') << '\n'; };
        auto hdr  = [&](const char* t){ line(); std::cout << center(t, W) << '\n'; line(); };
        auto row  = [&](const char* l, const std::string& v){
            std::cout << "  " << std::left << std::setw(28) << l << ": " << v << '\n';
        };

        // CPU
        hdr("CPU");
        char buf[128];
        snprintf(buf, 128, "%.1f%%", cs.totalUsage);
        row("Total Usage", buf);
        snprintf(buf, 128, "%.0f MHz", cs.frequency);
        row("Frequency", buf);
        snprintf(buf, 128, "%d / %d", cs.physicalCores, cs.logicalCores);
        row("Cores (phys/logical)", buf);
        snprintf(buf, 128, "%d", cs.totalThreads);
        row("System Threads", buf);
        if (cs.temperature > 0) {
            snprintf(buf, 128, "%.0f C", cs.temperature);
            row("Temperature", buf);
        }
        if (cs.loadAvg1 >= 0) {
            snprintf(buf, 128, "%.2f  %.2f  %.2f", cs.loadAvg1, cs.loadAvg5, cs.loadAvg15);
            row("Load Average (1/5/15)", buf);
        }
        snprintf(buf, 128, "%.1f%% (highest: %.1f%%)", cs.averageUsage, cs.highestUsage);
        row("Average / Highest", buf);

        // Memory
        hdr("MEMORY");
        snprintf(buf, 128, "%.1f%%  (%s / %s)",
                 ms.usagePercent,
                 fmtBytes(ms.usedBytes).c_str(),
                 fmtBytes(ms.totalBytes).c_str());
        row("Usage", buf);
        row("Cached", fmtBytes(ms.cachedBytes));
        snprintf(buf, 128, "%.1f%%  (%s / %s)",
                 ms.swapPercent,
                 fmtBytes(ms.swapUsed).c_str(),
                 fmtBytes(ms.swapTotal).c_str());
        row("Swap", buf);
        row("Top Process", ms.topProcessName);

        // Network
        hdr("NETWORK");
        row("Upload Rate", fmtRate(ns.totalUploadRate));
        row("Download Rate", fmtRate(ns.totalDownloadRate));
        row("Total Sent", fmtBytes(ns.totalBytesSent));
        row("Total Recv", fmtBytes(ns.totalBytesRecv));
        snprintf(buf, 128, "%d", static_cast<int>(ns.interfaces.size()));
        row("Interfaces", buf);

        // Disk
        if (!ds.disks.empty()) {
            hdr("DISK");
            for (auto& d : ds.disks) {
                snprintf(buf, 128, "%s  %s  %.1f%%  (%s / %s)  R:%s W:%s",
                         d.device.c_str(), d.mountPoint.c_str(), d.usagePercent,
                         fmtBytes(d.usedBytes).c_str(), fmtBytes(d.totalBytes).c_str(),
                         fmtRate(d.readBytesPerSec).c_str(),
                         fmtRate(d.writeBytesPerSec).c_str());
                std::cout << "  " << buf << '\n';
            }
        }

        // GPU
        if (!gs.gpus.empty()) {
            hdr("GPU");
            for (auto& g : gs.gpus) {
                snprintf(buf, 128, "%s  Util:%.0f%%  VRAM:%s/%s  Temp:%.0fC  Power:%.1fW",
                         g.name.c_str(), g.utilization,
                         fmtBytes(g.memoryUsed).c_str(), fmtBytes(g.memoryTotal).c_str(),
                         g.temperature, g.powerWatts);
                std::cout << "  " << buf << '\n';
            }
        }

        line();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\nMonitoring stopped.\n";
    db.exportToCSV();
    Logger::log("CLI terminated");
    return 0;
}
