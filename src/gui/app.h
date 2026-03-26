/**
 * @file app.h
 * @brief ImGui + ImPlot resource monitor application.
 *
 * All rendering is immediate-mode: each frame rebuilds the entire UI
 * from the latest MetricData snapshot.  A background collector thread
 * polls the OS at ~1 Hz, and the render loop runs at vsync (~60 fps).
 *
 * History buffers (ScrollingBuffer) hold up to 3 600 samples (1 hour
 * at 1 Hz).  ImPlot reads directly from the ring buffer via its
 * offset parameter — zero copies.
 */

#pragma once

#include "theme.h"
#include "implot.h"

#include "../core/metrics.h"
#include "../core/cpu/cpu_common.h"
#include "../core/memory/memory_common.h"
#include "../core/network/network_common.h"
#include "../core/disk/disk_common.h"
#include "../core/gpu/gpu_common.h"
#include "../core/process/process_common.h"
#include "../core/system_info/system_info.h"
#include "../core/alerts/alert_manager.h"
#include "../core/database/database.h"
#include "../utils/logger.h"
#include "../utils/scrolling_buffer.h"

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

struct GLFWwindow;

// ImPlot axis formatter: bytes/sec -> human-readable rate
static inline int RateFormatter(double value, char* buf, int size, void*) {
    const char* units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    double v = value < 0 ? -value : value;
    int u = 0;
    while (v >= 1024.0 && u < 3) { v /= 1024.0; u++; }
    if (value < 0) v = -v;
    snprintf(buf, size, "%.1f %s", v, units[u]);
    return static_cast<int>(strlen(buf));
}

// ImPlot axis formatter: bytes -> human-readable size
static inline int BytesAxisFormatter(double value, char* buf, int size, void*) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = value < 0 ? -value : value;
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    if (value < 0) v = -v;
    snprintf(buf, size, "%.1f %s", v, units[u]);
    return static_cast<int>(strlen(buf));
}

class App {
public:
    App();
    ~App();

    bool init();
    void run();
    void shutdown();

private:
    GLFWwindow* window_ = nullptr;

    // ---- Modules ------------------------------------------------------------
    std::unique_ptr<CPU>            cpu_;
    std::unique_ptr<Memory>         memory_;
    std::unique_ptr<Network>        network_;
    std::unique_ptr<Disk>           disk_;
    std::unique_ptr<GPU>            gpu_;
    std::unique_ptr<ProcessManager> process_;
    SystemInfo                      sysInfo_;
    AlertManager                    alerts_;
    Database                        db_;

    // ---- Shared state -------------------------------------------------------
    std::thread        collectorThread_;
    std::atomic<bool>  running_{false};
    mutable std::recursive_mutex dataMtx_;
    MetricData         latest_;
    float              elapsedTime_ = 0.0f;

    // ---- History buffers ----------------------------------------------------
    ScrollingBuffer hCpu_, hMem_, hSwap_;
    ScrollingBuffer hNetUp_, hNetDown_;
    ScrollingBuffer hDiskRead_, hDiskWrite_;
    ScrollingBuffer hGpuUtil_, hGpuTemp_, hGpuMem_;
    std::vector<ScrollingBuffer> hCores_;

    // ---- UI state -----------------------------------------------------------
    int  currentTab_        = 0;
    bool showDemoWindow_    = false;
    bool dbEnabled_         = true;
    int  dbIntervalTicks_   = 10;
    int  tickCounter_       = 0;

    // Process tab
    char processFilter_[128] = {};
    int  sortCol_          = 4;
    bool sortAsc_          = false;
    int  selectedPid_      = -1;

    // Alert tab
    char newAlertName_[64]  = {};
    int  newAlertMetric_    = 0;
    float newAlertThresh_   = 90.0f;
    bool  newAlertAbove_    = true;
    int   newAlertSustain_  = 5;

    // Export controls (System tab)
    int  exportTimeframe_   = 1;   // 0=1h, 1=24h, 2=7d, 3=30d
    bool exportCpu_ = true, exportMem_ = true, exportNet_ = true;
    bool exportDisk_ = true, exportGpu_ = true;
    int  exportFormat_      = 0;   // 0=CSV, 1=TXT
    char exportStatus_[128] = {};

    // ---- Methods ------------------------------------------------------------
    void collectorLoop();
    void render();
    void renderMenuBar();
    void renderOverview();
    void renderCpuTab();
    void renderMemoryTab();
    void renderNetworkTab();
    void renderDiskTab();
    void renderGpuTab();
    void renderProcessTab();
    void renderAlertTab();
    void renderSystemTab();

    void plotLine(const char* label, ScrollingBuffer& buf, float tNow,
                  float histSec = 60.0f, const ImVec4& col = Theme::AccentBlue);
    void plotShaded(const char* label, ScrollingBuffer& buf, float tNow,
                    float histSec = 60.0f, const ImVec4& col = Theme::AccentBlue);
    void bigNumber(const char* label, float value, const char* fmt = "%.1f%%");
};

// ===========================================================================
//  I N L I N E   I M P L E M E N T A T I O N
// ===========================================================================

inline App::App()
    : db_("resource_monitor.db")
{}

inline App::~App() { shutdown(); }

// ---------------------------------------------------------------------------
//  Collector thread
// ---------------------------------------------------------------------------
inline void App::collectorLoop() {
    using clock = std::chrono::steady_clock;

    auto doUpdate = [&]() {
        if (cpu_)     cpu_->update();
        if (memory_)  memory_->update();
        if (network_) network_->update();
        if (disk_)    disk_->update();
        if (gpu_)     gpu_->update();
        if (process_) process_->update();
        sysInfo_.update();
    };

    doUpdate();

    while (running_) {
        auto t0 = clock::now();
        doUpdate();

        MetricData md;
        if (cpu_)     md.cpu     = cpu_->snapshot();
        if (memory_)  md.memory  = memory_->snapshot();
        if (network_) md.network = network_->snapshot();
        if (disk_)    md.disk    = disk_->snapshot();
        if (gpu_)     md.gpu     = gpu_->snapshot();
        if (process_) md.process = process_->snapshot();
        md.systemInfo = sysInfo_.snapshot();

        alerts_.evaluate(md);

        float t = elapsedTime_;

        {
            std::lock_guard<std::recursive_mutex> lk(dataMtx_);
            latest_ = md;
            elapsedTime_ += 1.0f;

            hCpu_.AddPoint(t, md.cpu.totalUsage);
            hMem_.AddPoint(t, md.memory.usagePercent);
            hSwap_.AddPoint(t, md.memory.swapPercent);
            hNetUp_.AddPoint(t, md.network.totalUploadRate);
            hNetDown_.AddPoint(t, md.network.totalDownloadRate);
            hDiskRead_.AddPoint(t, md.disk.totalReadRate);
            hDiskWrite_.AddPoint(t, md.disk.totalWriteRate);

            if (!md.gpu.gpus.empty()) {
                hGpuUtil_.AddPoint(t, md.gpu.gpus[0].utilization);
                hGpuTemp_.AddPoint(t, md.gpu.gpus[0].temperature);
                hGpuMem_.AddPoint(t, md.gpu.gpus[0].memoryPercent);
            }

            int nc = static_cast<int>(md.cpu.cores.size());
            if (static_cast<int>(hCores_.size()) < nc)
                hCores_.resize(nc, ScrollingBuffer(3600));
            for (int i = 0; i < nc; ++i)
                hCores_[i].AddPoint(t, md.cpu.cores[i].usage);
        }

        ++tickCounter_;
        if (dbEnabled_ && tickCounter_ >= dbIntervalTicks_) {
            tickCounter_ = 0;
            db_.insertSnapshot(md);
        }

        auto dt = clock::now() - t0;
        auto remaining = std::chrono::seconds(1) - dt;
        if (remaining.count() > 0)
            std::this_thread::sleep_for(remaining);
    }
}

// ---------------------------------------------------------------------------
//  Tiny helpers
// ---------------------------------------------------------------------------

inline void App::plotLine(const char* label, ScrollingBuffer& buf,
                          float tNow, float histSec, const ImVec4& col) {
    if (buf.Size() == 0) return;
    ImPlot::SetNextLineStyle(col, 2.0f);
    ImPlot::PlotLine(label, buf.DataX.data(), buf.DataY.data(),
                     buf.Size(), ImPlotLineFlags_None, buf.Offset, sizeof(float));
}

inline void App::plotShaded(const char* label, ScrollingBuffer& buf,
                            float tNow, float histSec, const ImVec4& col) {
    if (buf.Size() == 0) return;
    ImPlot::SetNextFillStyle(col, 0.15f);
    ImPlot::PlotShaded(label, buf.DataX.data(), buf.DataY.data(),
                       buf.Size(), 0, ImPlotShadedFlags_None, buf.Offset, sizeof(float));
    plotLine(label, buf, tNow, histSec, col);
}

inline void App::bigNumber(const char* label, float value, const char* fmt) {
    ImGui::PushFont(nullptr);
    char valBuf[64];
    snprintf(valBuf, sizeof(valBuf), fmt, value);
    ImGui::TextColored(Theme::SeverityColor(value), "%s", valBuf);
    ImGui::TextColored(Theme::TextSecondary, "%s", label);
    ImGui::PopFont();
}

// ---------------------------------------------------------------------------
//  RENDER
// ---------------------------------------------------------------------------

inline void App::render() {
    MetricData data;
    float tNow;
    {
        std::lock_guard<std::recursive_mutex> lk(dataMtx_);
        data  = latest_;
        tNow  = elapsedTime_;
    }

    renderMenuBar();

    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize -
                             ImVec2(0, ImGui::GetFrameHeight()));
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_Reorderable)) {
        if (ImGui::BeginTabItem("Overview"))   { renderOverview();    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("CPU"))        { renderCpuTab();      ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Memory"))     { renderMemoryTab();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Network"))    { renderNetworkTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Disk"))       { renderDiskTab();     ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("GPU"))        { renderGpuTab();      ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Processes"))  { renderProcessTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Alerts"))     { renderAlertTab();    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("System"))     { renderSystemTab();   ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::End();
    if (showDemoWindow_) ImGui::ShowDemoWindow(&showDemoWindow_);
}

// ---------------------------------------------------------------------------
//  Menu bar — adaptive status text
// ---------------------------------------------------------------------------

inline void App::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Export CSV"))      db_.exportToCSV();
            if (ImGui::MenuItem("Prune (7 days)"))  db_.pruneOlderThan(7);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))            running_ = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            ImGui::Checkbox("Database logging", &dbEnabled_);
            ImGui::SliderInt("DB write interval (ticks)", &dbIntervalTicks_, 1, 60);
            ImGui::EndMenu();
        }

        // Right-align status info — compute width dynamically
        MetricData snap;
        { std::lock_guard<std::recursive_mutex> lk(dataMtx_); snap = latest_; }
        char statusBuf[256];
        char ub[32], db2[32];
        Theme::FormatRate(snap.network.totalUploadRate, ub, 32);
        Theme::FormatRate(snap.network.totalDownloadRate, db2, 32);
        snprintf(statusBuf, sizeof(statusBuf),
            "CPU %.0f%%  |  Mem %.0f%%  |  Up %s  |  Down %s",
            snap.cpu.totalUsage, snap.memory.usagePercent, ub, db2);
        float textW = ImGui::CalcTextSize(statusBuf).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - textW - 16.0f);
        ImGui::TextColored(Theme::TextSecondary, "%s", statusBuf);

        ImGui::EndMainMenuBar();
    }
}

// ---------------------------------------------------------------------------
//  OVERVIEW — 2x3 grid, auto-sized cards
// ---------------------------------------------------------------------------

inline void App::renderOverview() {
    MetricData d;
    float t;
    { std::lock_guard<std::recursive_mutex> lk(dataMtx_); d = latest_; t = elapsedTime_; }

    float cardW = (ImGui::GetContentRegionAvail().x - 20) / 3.0f;
    float cardH = (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y) / 2.0f;

    auto card = [&](const char* title, float pct, ScrollingBuffer& buf,
                    const char* detail, const ImVec4& col) {
        Theme::BeginCard(title, cardW, cardH);
        ImGui::TextColored(Theme::TextPrimary, "%s", title);
        char pctBuf[32]; snprintf(pctBuf, 32, "%.1f%%", pct);
        ImGui::SameLine(cardW - 70);
        ImGui::TextColored(Theme::SeverityColor(pct), "%s", pctBuf);
        ImGui::TextColored(Theme::TextSecondary, "%s", detail);

        if (ImPlot::BeginPlot("##plot", ImVec2(-1, -1),
                              ImPlotFlags_CanvasOnly | ImPlotFlags_NoInputs)) {
            ImPlot::SetupAxes(nullptr, nullptr,
                              ImPlotAxisFlags_NoDecorations,
                              ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_LockMin);
            float xMin = t - 60.0f; if (xMin < 0) xMin = 0;
            ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
            plotShaded("##v", buf, t, 60, col);
            ImPlot::EndPlot();
        }
        Theme::EndCard();
    };

    // Row 1: CPU, Memory, Swap
    char cpuDetail[128]; snprintf(cpuDetail, 128, "%d cores  %.0f MHz",
                                  d.cpu.logicalCores, d.cpu.frequency);
    card("CPU", d.cpu.totalUsage, hCpu_, cpuDetail, Theme::AccentBlue);
    ImGui::SameLine();

    char memDetail[128];
    { char u[32], t2[32];
      Theme::FormatBytes(d.memory.usedBytes, u, 32);
      Theme::FormatBytes(d.memory.totalBytes, t2, 32);
      snprintf(memDetail, 128, "%s / %s", u, t2); }
    card("Memory", d.memory.usagePercent, hMem_, memDetail, Theme::AccentCyan);
    ImGui::SameLine();

    char swapDetail[128];
    { char u[32], t2[32];
      Theme::FormatBytes(d.memory.swapUsed, u, 32);
      Theme::FormatBytes(d.memory.swapTotal, t2, 32);
      snprintf(swapDetail, 128, "Swap: %s / %s", u, t2); }
    card("Swap", d.memory.swapPercent, hSwap_, swapDetail, Theme::AccentOrange);

    // Row 2: Network (custom 2-line), Disk (custom 2-line), GPU
    // --- Network card ---
    Theme::BeginCard("Network", cardW, cardH);
    ImGui::TextColored(Theme::TextPrimary, "Network");
    char netDetail[128];
    { char u[32], dn[32];
      Theme::FormatRate(d.network.totalUploadRate, u, 32);
      Theme::FormatRate(d.network.totalDownloadRate, dn, 32);
      snprintf(netDetail, 128, "Up: %s  Down: %s", u, dn); }
    ImGui::TextColored(Theme::TextSecondary, "%s", netDetail);

    if (ImPlot::BeginPlot("##netOv", ImVec2(-1, -1),
                          ImPlotFlags_NoInputs | ImPlotFlags_NoMenus |
                          ImPlotFlags_NoBoxSelect | ImPlotFlags_NoTitle)) {
        float xMin = t - 60.0f; if (xMin < 0) xMin = 0;
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_NoDecorations,
                          ImPlotAxisFlags_NoLabel);
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        float netMax = std::max(hNetUp_.MaxYInWindow(xMin),
                                hNetDown_.MaxYInWindow(xMin));
        if (netMax < 1024.0f) netMax = 1024.0f;
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, netMax * 1.15, ImGuiCond_Always);
        ImPlot::SetupAxisFormat(ImAxis_Y1, RateFormatter, nullptr);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        ImPlot::SetNextFillStyle(Theme::AccentGreen, 0.08f);
        plotLine("Upload",   hNetUp_,   t, 60, Theme::AccentGreen);
        ImPlot::SetNextFillStyle(Theme::AccentPurple, 0.08f);
        plotLine("Download", hNetDown_, t, 60, Theme::AccentPurple);
        ImPlot::EndPlot();
    }
    Theme::EndCard();
    ImGui::SameLine();

    // --- Disk card ---
    float diskPct = 0;
    if (!d.disk.disks.empty()) diskPct = d.disk.disks[0].usagePercent;

    Theme::BeginCard("Disk", cardW, cardH);
    ImGui::TextColored(Theme::TextPrimary, "Disk");
    char diskDetail[128];
    { char r[32], w[32];
      Theme::FormatRate(d.disk.totalReadRate, r, 32);
      Theme::FormatRate(d.disk.totalWriteRate, w, 32);
      snprintf(diskDetail, 128, "R: %s  W: %s", r, w); }
    ImGui::SameLine(cardW - 70);
    ImGui::TextColored(Theme::SeverityColor(diskPct), "%.1f%%", diskPct);
    ImGui::TextColored(Theme::TextSecondary, "%s", diskDetail);

    if (ImPlot::BeginPlot("##diskOv", ImVec2(-1, -1),
                          ImPlotFlags_NoInputs | ImPlotFlags_NoMenus |
                          ImPlotFlags_NoBoxSelect | ImPlotFlags_NoTitle)) {
        float xMin = t - 60.0f; if (xMin < 0) xMin = 0;
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_NoDecorations,
                          ImPlotAxisFlags_NoLabel);
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        float diskMax = std::max(hDiskRead_.MaxYInWindow(xMin),
                                 hDiskWrite_.MaxYInWindow(xMin));
        if (diskMax < 1024.0f) diskMax = 1024.0f;
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, diskMax * 1.15, ImGuiCond_Always);
        ImPlot::SetupAxisFormat(ImAxis_Y1, RateFormatter, nullptr);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);
        plotLine("Read",  hDiskRead_,  t, 60, Theme::AccentGreen);
        plotLine("Write", hDiskWrite_, t, 60, Theme::AccentOrange);
        ImPlot::EndPlot();
    }
    Theme::EndCard();
    ImGui::SameLine();

    // --- GPU card ---
    float gpuPct = 0;
    char gpuDetail[128] = "No GPU detected";
    if (!d.gpu.gpus.empty()) {
        gpuPct = d.gpu.gpus[0].utilization;
        snprintf(gpuDetail, 128, "%.0f C  %.0f W  %s",
                 d.gpu.gpus[0].temperature, d.gpu.gpus[0].powerWatts,
                 d.gpu.gpus[0].name.c_str());
    }
    card("GPU", gpuPct, hGpuUtil_, gpuDetail, Theme::AccentRed);
}

// ---------------------------------------------------------------------------
//  CPU TAB — enriched with model, cache, better layout
// ---------------------------------------------------------------------------

inline void App::renderCpuTab() {
    MetricData d; float t;
    { std::lock_guard<std::recursive_mutex> lk(dataMtx_); d = latest_; t = elapsedTime_; }

    // Summary panel
    ImGui::TextColored(Theme::TextPrimary,
        "CPU Usage: %.1f%%  |  Freq: %.0f MHz  |  Phys: %d  |  Logical: %d  |  Threads: %d",
        d.cpu.totalUsage, d.cpu.frequency, d.cpu.physicalCores, d.cpu.logicalCores, d.cpu.totalThreads);

    // Second line: model, arch, temperature, cache
    auto& s = d.systemInfo;
    if (!s.cpuModel.empty())
        ImGui::TextColored(Theme::TextSecondary, "Model: %s  |  Arch: %s",
            s.cpuModel.c_str(), s.arch.c_str());

    if (d.cpu.temperature > 0 || s.l3CacheKB > 0) {
        char infoLine[256] = {};
        int pos = 0;
        if (d.cpu.temperature > 0)
            pos += snprintf(infoLine + pos, 256 - pos, "Temp: %.0f C", d.cpu.temperature);
        if (s.l1CacheKB > 0) {
            if (pos > 0) pos += snprintf(infoLine + pos, 256 - pos, "  |  ");
            pos += snprintf(infoLine + pos, 256 - pos, "L1: %u KB  L2: %u KB  L3: %u KB",
                s.l1CacheKB, s.l2CacheKB, s.l3CacheKB);
        }
        ImGui::TextColored(Theme::TextSecondary, "%s", infoLine);
    }

    if (d.cpu.loadAvg1 >= 0)
        ImGui::TextColored(Theme::TextSecondary,
            "Load: %.2f  %.2f  %.2f  |  Ctx/s: %.0f  |  IRQ/s: %.0f",
            d.cpu.loadAvg1, d.cpu.loadAvg5, d.cpu.loadAvg15,
            d.cpu.contextSwitchesPerSec, d.cpu.interruptsPerSec);

    // Usage breakdown
    ImGui::TextColored(Theme::TextSecondary,
        "User: %.1f%%  |  System: %.1f%%  |  Idle: %.1f%%  |  Avg: %.1f%%  |  Peak: %.1f%%",
        d.cpu.userPercent, d.cpu.systemPercent, d.cpu.idlePercent,
        d.cpu.averageUsage, d.cpu.highestUsage);

    ImGui::Separator();

    float xMin = t - 120; if (xMin < 0) xMin = 0;
    float avail = ImGui::GetContentRegionAvail().y;
    int nc = static_cast<int>(d.cpu.cores.size());

    // Total CPU usage graph
    float h1 = avail * 0.35f;
    if (h1 < 100) h1 = 100;
    if (ImPlot::BeginPlot("Total CPU Usage", ImVec2(-1, h1))) {
        ImPlot::SetupAxes("Time (s)", "%", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_LockMin);
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
        plotShaded("Usage", hCpu_, t, 120, Theme::AccentBlue);
        ImPlot::EndPlot();
    }

    // Per-core graph with outside legend
    float h2 = avail * 0.35f;
    if (h2 < 120) h2 = 120;
    if (nc > 0 && ImPlot::BeginPlot("Per-Core Usage", ImVec2(-1, h2))) {
        ImPlot::SetupAxes("Time (s)", "%");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);

        std::lock_guard<std::recursive_mutex> lk(dataMtx_);
        for (int i = 0; i < nc && i < static_cast<int>(hCores_.size()); ++i) {
            char lbl[16]; snprintf(lbl, 16, "Core %d", i);
            ImPlot::SetNextLineStyle(Theme::CoreColor(i), 1.5f);
            ImPlot::PlotLine(lbl, hCores_[i].DataX.data(), hCores_[i].DataY.data(),
                             hCores_[i].Size(), ImPlotLineFlags_None,
                             hCores_[i].Offset, sizeof(float));
        }
        ImPlot::EndPlot();
    }

    // Per-core bar chart (current snapshot)
    float h3 = avail * 0.25f;
    if (h3 < 80) h3 = 80;
    if (nc > 0 && ImPlot::BeginPlot("Current Per-Core", ImVec2(-1, h3))) {
        ImPlot::SetupAxes("Core", "%");
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
        std::vector<float> xs(nc), ys(nc);
        for (int i = 0; i < nc; ++i) { xs[i] = (float)i; ys[i] = d.cpu.cores[i].usage; }
        ImPlot::PlotBars("Usage", xs.data(), ys.data(), nc, 0.6);
        ImPlot::EndPlot();
    }
}

// ---------------------------------------------------------------------------
//  MEMORY TAB — enriched with commit charge, pools, top consumers
// ---------------------------------------------------------------------------

inline void App::renderMemoryTab() {
    MetricData d; float t;
    { std::lock_guard<std::recursive_mutex> lk(dataMtx_); d = latest_; t = elapsedTime_; }

    char u[32], a[32], tot[32], c[32], b[32];
    Theme::FormatBytes(d.memory.usedBytes, u, 32);
    Theme::FormatBytes(d.memory.availableBytes, a, 32);
    Theme::FormatBytes(d.memory.totalBytes, tot, 32);
    Theme::FormatBytes(d.memory.cachedBytes, c, 32);
    Theme::FormatBytes(d.memory.bufferedBytes, b, 32);

    ImGui::TextColored(Theme::TextPrimary,
        "Usage: %.1f%%  |  Used: %s  |  Available: %s  |  Total: %s",
        d.memory.usagePercent, u, a, tot);

    // Commit charge + pools
    char cm[32], cl[32];
    Theme::FormatBytes(d.memory.committedBytes, cm, 32);
    Theme::FormatBytes(d.memory.commitLimitBytes, cl, 32);
    ImGui::TextColored(Theme::TextSecondary,
        "Committed: %s / %s  |  Cached: %s  |  Buffers: %s",
        cm, cl, c, b);

    if (d.memory.pagedPoolBytes > 0 || d.memory.nonPagedPoolBytes > 0) {
        char pp[32], np[32];
        Theme::FormatBytes(d.memory.pagedPoolBytes, pp, 32);
        Theme::FormatBytes(d.memory.nonPagedPoolBytes, np, 32);
        ImGui::TextColored(Theme::TextSecondary,
            "Paged Pool: %s  |  Non-Paged Pool: %s  |  Page Faults/s: %.0f",
            pp, np, d.memory.pageFaultsPerSec > 0 ? d.memory.pageFaultsPerSec : 0.0f);
    }

    ImGui::TextColored(Theme::TextSecondary,
        "Top: %s  |  Avg Usage: %.1f%%",
        d.memory.topProcessName.c_str(), d.memory.averageUsage);

    ImGui::Separator();

    float xMin = t - 120; if (xMin < 0) xMin = 0;
    float avail = ImGui::GetContentRegionAvail().y;

    // RAM usage graph
    float h1 = avail * 0.35f;
    if (h1 < 100) h1 = 100;
    if (ImPlot::BeginPlot("Memory Usage", ImVec2(-1, h1))) {
        ImPlot::SetupAxes("Time (s)", "%");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
        plotShaded("RAM", hMem_, t, 120, Theme::AccentCyan);
        ImPlot::EndPlot();
    }

    // Swap
    char su[32], st[32];
    Theme::FormatBytes(d.memory.swapUsed, su, 32);
    Theme::FormatBytes(d.memory.swapTotal, st, 32);
    ImGui::TextColored(Theme::TextPrimary,
        "Swap: %.1f%%  |  Used: %s  |  Total: %s",
        d.memory.swapPercent, su, st);

    float h2 = avail * 0.25f;
    if (h2 < 80) h2 = 80;
    if (ImPlot::BeginPlot("Swap Usage", ImVec2(-1, h2))) {
        ImPlot::SetupAxes("Time (s)", "%");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
        plotShaded("Swap", hSwap_, t, 120, Theme::AccentOrange);
        ImPlot::EndPlot();
    }

    // Composition bar
    float totalMB = d.memory.totalBytes / (1024.f * 1024.f);
    float usedMB  = d.memory.usedBytes  / (1024.f * 1024.f);
    float cacheMB = d.memory.cachedBytes / (1024.f * 1024.f);
    float bufMB   = d.memory.bufferedBytes / (1024.f * 1024.f);
    if (totalMB > 0) {
        ImGui::TextColored(Theme::TextPrimary, "Composition:");
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Theme::AccentCyan);
        ImGui::ProgressBar(usedMB / totalMB, ImVec2(-1, 18), "Used");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Theme::AccentGreen);
        ImGui::ProgressBar(cacheMB / totalMB, ImVec2(-1, 18), "Cached");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Theme::AccentPurple);
        ImGui::ProgressBar(bufMB / totalMB, ImVec2(-1, 18), "Buffers");
        ImGui::PopStyleColor();
    }

    // Top memory consumers table
    if (!d.memory.topProcesses.empty()) {
        ImGui::Separator();
        ImGui::TextColored(Theme::TextPrimary, "Top Memory Consumers");
        if (ImGui::BeginTable("##topmem", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg,
                              ImVec2(400, 0))) {
            ImGui::TableSetupColumn("Process"); ImGui::TableSetupColumn("Memory");
            ImGui::TableHeadersRow();
            for (auto& tp : d.memory.topProcesses) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", tp.name.c_str());
                char mb[32];
                ImGui::TableNextColumn(); ImGui::Text("%s", Theme::FormatBytes(tp.memoryBytes, mb, 32));
            }
            ImGui::EndTable();
        }
    }
}

// ---------------------------------------------------------------------------
//  NETWORK TAB — with rate-formatted Y axis
// ---------------------------------------------------------------------------

inline void App::renderNetworkTab() {
    MetricData d; float t;
    { std::lock_guard<std::recursive_mutex> lk(dataMtx_); d = latest_; t = elapsedTime_; }

    char up[32], dn[32], ts[32], tr[32];
    Theme::FormatRate(d.network.totalUploadRate, up, 32);
    Theme::FormatRate(d.network.totalDownloadRate, dn, 32);
    Theme::FormatBytes(d.network.totalBytesSent, ts, 32);
    Theme::FormatBytes(d.network.totalBytesRecv, tr, 32);

    ImGui::TextColored(Theme::TextPrimary,
        "Upload: %s  |  Download: %s  |  Total Sent: %s  |  Total Recv: %s  |  Interfaces: %d",
        up, dn, ts, tr, (int)d.network.interfaces.size());

    ImGui::Separator();

    float xMin = t - 120; if (xMin < 0) xMin = 0;
    float avail = ImGui::GetContentRegionAvail().y;

    // Upload graph
    float graphH = avail * 0.2f;
    if (graphH < 100) graphH = 100;

    if (ImPlot::BeginPlot("Upload Rate", ImVec2(-1, graphH))) {
        ImPlot::SetupAxes("Time (s)", "Rate");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisFormat(ImAxis_Y1, RateFormatter, nullptr);
        plotShaded("Upload", hNetUp_, t, 120, Theme::AccentGreen);
        ImPlot::EndPlot();
    }

    // Download graph
    if (ImPlot::BeginPlot("Download Rate", ImVec2(-1, graphH))) {
        ImPlot::SetupAxes("Time (s)", "Rate");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisFormat(ImAxis_Y1, RateFormatter, nullptr);
        plotShaded("Download", hNetDown_, t, 120, Theme::AccentPurple);
        ImPlot::EndPlot();
    }

    // Interface table
    if (!d.network.interfaces.empty() &&
        ImGui::BeginTable("##ifaces", 8,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("IP");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Speed");
        ImGui::TableSetupColumn("Upload");
        ImGui::TableSetupColumn("Download");
        ImGui::TableSetupColumn("Errors In");
        ImGui::TableSetupColumn("Drops In");
        ImGui::TableHeadersRow();

        for (auto& iface : d.network.interfaces) {
            if (!iface.isUp && iface.uploadRate == 0 && iface.downloadRate == 0)
                continue; // skip inactive interfaces
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", iface.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", iface.ipAddress.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(iface.isUp ? Theme::AccentGreen : Theme::AccentRed,
                               "%s", iface.isUp ? "UP" : "DOWN");
            ImGui::TableNextColumn(); ImGui::Text("%.0f Mbps", iface.linkSpeedMbps);
            char ub2[32], db2[32];
            ImGui::TableNextColumn(); ImGui::Text("%s", Theme::FormatRate(iface.uploadRate, ub2, 32));
            ImGui::TableNextColumn(); ImGui::Text("%s", Theme::FormatRate(iface.downloadRate, db2, 32));
            ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)iface.errorsIn);
            ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)iface.dropsIn);
        }
        ImGui::EndTable();
    }

    // Connections table
    if (!d.network.connections.empty()) {
        ImGui::TextColored(Theme::TextPrimary,
            "TCP Connections (%d)", (int)d.network.connections.size());
        if (ImGui::BeginTable("##conns", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                ImVec2(0, 200))) {
            ImGui::TableSetupColumn("Local");
            ImGui::TableSetupColumn("Remote");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("PID");
            ImGui::TableSetupColumn("Process");
            ImGui::TableHeadersRow();

            for (auto& conn : d.network.connections) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s:%d", conn.localAddr.c_str(), conn.localPort);
                ImGui::TableNextColumn(); ImGui::Text("%s:%d", conn.remoteAddr.c_str(), conn.remotePort);
                ImGui::TableNextColumn(); ImGui::Text("%s", conn.state.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%d", conn.pid);
                ImGui::TableNextColumn(); ImGui::Text("%s", conn.processName.c_str());
            }
            ImGui::EndTable();
        }
    }
}

// ---------------------------------------------------------------------------
//  DISK TAB — separate read/write graphs, IOPS, utilization
// ---------------------------------------------------------------------------

inline void App::renderDiskTab() {
    MetricData d; float t;
    { std::lock_guard<std::recursive_mutex> lk(dataMtx_); d = latest_; t = elapsedTime_; }

    char r[32], w[32];
    Theme::FormatRate(d.disk.totalReadRate, r, 32);
    Theme::FormatRate(d.disk.totalWriteRate, w, 32);
    ImGui::TextColored(Theme::TextPrimary, "Total Read: %s  |  Total Write: %s", r, w);

    ImGui::Separator();

    float xMin = t - 120; if (xMin < 0) xMin = 0;
    float avail = ImGui::GetContentRegionAvail().y;

    // Read rate graph
    float graphH = avail * 0.2f;
    if (graphH < 100) graphH = 100;

    if (ImPlot::BeginPlot("Read Rate", ImVec2(-1, graphH))) {
        ImPlot::SetupAxes("Time (s)", "Rate");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisFormat(ImAxis_Y1, RateFormatter, nullptr);
        plotShaded("Read", hDiskRead_, t, 120, Theme::AccentGreen);
        ImPlot::EndPlot();
    }

    // Write rate graph
    if (ImPlot::BeginPlot("Write Rate", ImVec2(-1, graphH))) {
        ImPlot::SetupAxes("Time (s)", "Rate");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisFormat(ImAxis_Y1, RateFormatter, nullptr);
        plotShaded("Write", hDiskWrite_, t, 120, Theme::AccentOrange);
        ImPlot::EndPlot();
    }

    // Per-volume table with IOPS and utilization
    if (!d.disk.disks.empty() &&
        ImGui::BeginTable("##disks", 10,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable,
            ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Device");
        ImGui::TableSetupColumn("Mount");
        ImGui::TableSetupColumn("FS");
        ImGui::TableSetupColumn("Total");
        ImGui::TableSetupColumn("Used");
        ImGui::TableSetupColumn("Free");
        ImGui::TableSetupColumn("Use%");
        ImGui::TableSetupColumn("Read IOPS");
        ImGui::TableSetupColumn("Write IOPS");
        ImGui::TableSetupColumn("Util%");
        ImGui::TableHeadersRow();

        for (auto& dsk : d.disk.disks) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", dsk.device.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", dsk.mountPoint.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", dsk.fsType.c_str());
            char tb[32], ub[32], fb[32];
            ImGui::TableNextColumn(); ImGui::Text("%s", Theme::FormatBytes(dsk.totalBytes, tb, 32));
            ImGui::TableNextColumn(); ImGui::Text("%s", Theme::FormatBytes(dsk.usedBytes, ub, 32));
            ImGui::TableNextColumn(); ImGui::Text("%s", Theme::FormatBytes(dsk.freeBytes, fb, 32));
            ImGui::TableNextColumn();
            ImGui::TextColored(Theme::SeverityColor(dsk.usagePercent), "%.1f%%", dsk.usagePercent);
            ImGui::TableNextColumn(); ImGui::Text("%.0f", dsk.readOpsPerSec);
            ImGui::TableNextColumn(); ImGui::Text("%.0f", dsk.writeOpsPerSec);
            ImGui::TableNextColumn();
            if (dsk.utilizationPct >= 0)
                ImGui::TextColored(Theme::SeverityColor(dsk.utilizationPct), "%.1f%%", dsk.utilizationPct);
            else
                ImGui::TextColored(Theme::TextSecondary, "N/A");
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
//  GPU TAB — VRAM graph, multi-vendor support display
// ---------------------------------------------------------------------------

inline void App::renderGpuTab() {
    MetricData d; float t;
    { std::lock_guard<std::recursive_mutex> lk(dataMtx_); d = latest_; t = elapsedTime_; }

    if (d.gpu.gpus.empty()) {
        ImGui::TextColored(Theme::TextSecondary, "No GPU detected.");
        return;
    }

    for (size_t gi = 0; gi < d.gpu.gpus.size(); ++gi) {
        auto& g = d.gpu.gpus[gi];
        ImGui::TextColored(Theme::TextPrimary, "GPU %d: %s", (int)gi, g.name.c_str());

        char mu[32], mt[32];
        Theme::FormatBytes(g.memoryUsed, mu, 32);
        Theme::FormatBytes(g.memoryTotal, mt, 32);

        if (g.utilization >= 0 || g.temperature >= 0) {
            char fanStr[16];
            if (g.fanPercent < 0) snprintf(fanStr, sizeof(fanStr), "N/A");
            else                  snprintf(fanStr, sizeof(fanStr), "%.0f%%", g.fanPercent);
            ImGui::TextColored(Theme::TextSecondary,
                "Util: %.0f%%  |  VRAM: %s / %s (%.1f%%)  |  Temp: %.0f C  |  Power: %.1f W  |  Fan: %s  |  Clock: %.0f MHz",
                g.utilization, mu, mt, g.memoryPercent,
                g.temperature, g.powerWatts, fanStr, g.clockMHz);
        } else {
            // Basic DXGI info only
            ImGui::TextColored(Theme::TextSecondary,
                "VRAM: %s  |  Driver: %s  |  (Limited telemetry — vendor SDK not available)",
                mt, g.driver.c_str());
        }

        if (!g.driver.empty())
            ImGui::TextColored(Theme::TextSecondary, "Driver: %s  |  Mem Clock: %.0f MHz",
                g.driver.c_str(), g.memClockMHz);
    }

    ImGui::Separator();

    float xMin = t - 120; if (xMin < 0) xMin = 0;
    float avail = ImGui::GetContentRegionAvail().y;

    // Utilization graph
    float h1 = avail * 0.3f;
    if (h1 < 100) h1 = 100;
    if (ImPlot::BeginPlot("GPU Utilization", ImVec2(-1, h1))) {
        ImPlot::SetupAxes("Time (s)", "%");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
        plotShaded("Utilization", hGpuUtil_, t, 120, Theme::AccentRed);
        ImPlot::EndPlot();
    }

    // VRAM usage graph
    float h2 = avail * 0.3f;
    if (h2 < 80) h2 = 80;
    if (ImPlot::BeginPlot("VRAM Usage", ImVec2(-1, h2))) {
        ImPlot::SetupAxes("Time (s)", "%");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
        plotShaded("VRAM", hGpuMem_, t, 120, Theme::AccentCyan);
        ImPlot::EndPlot();
    }

    // Temperature graph
    float h3 = avail * 0.25f;
    if (h3 < 80) h3 = 80;
    if (ImPlot::BeginPlot("GPU Temperature", ImVec2(-1, h3))) {
        ImPlot::SetupAxes("Time (s)", "C");
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, t, ImGuiCond_Always);
        plotLine("Temp", hGpuTemp_, t, 120, Theme::AccentOrange);
        ImPlot::EndPlot();
    }
}

// ---------------------------------------------------------------------------
//  PROCESS TAB
// ---------------------------------------------------------------------------

inline void App::renderProcessTab() {
    MetricData d;
    { std::lock_guard<std::recursive_mutex> lk(dataMtx_); d = latest_; }

    ImGui::TextColored(Theme::TextPrimary,
        "Processes: %d  |  Threads: %d  |  Running: %d",
        d.process.totalProcesses, d.process.totalThreads,
        d.process.runningProcesses);

    ImGui::InputTextWithHint("##filter", "Filter by name...",
                             processFilter_, sizeof(processFilter_));
    ImGui::SameLine();
    if (ImGui::Button("Kill Selected") && selectedPid_ > 0) {
        if (process_) process_->killProcess(selectedPid_);
    }

    std::vector<const ProcessInfo*> filtered;
    for (auto& p : d.process.processes) {
        if (processFilter_[0] &&
            p.name.find(processFilter_) == std::string::npos)
            continue;
        filtered.push_back(&p);
    }

    auto cmp = [&](const ProcessInfo* lhs, const ProcessInfo* rhs) -> bool {
        const ProcessInfo* a = sortAsc_ ? lhs : rhs;
        const ProcessInfo* b = sortAsc_ ? rhs : lhs;
        switch (sortCol_) {
            case 0: return a->pid < b->pid;
            case 1: return a->name < b->name;
            case 2: return a->state < b->state;
            case 3: return a->memoryBytes < b->memoryBytes;
            case 4: return a->cpuPercent < b->cpuPercent;
            case 5: return a->threads < b->threads;
            default: return a->pid < b->pid;
        }
    };
    std::sort(filtered.begin(), filtered.end(), cmp);

    if (ImGui::BeginTable("##procs", 8,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
            ImGuiTableFlags_ScrollY,
            ImVec2(0, -1))) {
        ImGui::TableSetupColumn("PID",      ImGuiTableColumnFlags_DefaultSort, 0, 0);
        ImGui::TableSetupColumn("Name",     0, 0, 1);
        ImGui::TableSetupColumn("State",    0, 0, 2);
        ImGui::TableSetupColumn("Memory",   0, 0, 3);
        ImGui::TableSetupColumn("CPU%",     ImGuiTableColumnFlags_DefaultSort, 0, 4);
        ImGui::TableSetupColumn("Threads",  0, 0, 5);
        ImGui::TableSetupColumn("Priority", 0, 0, 6);
        ImGui::TableSetupColumn("User",     0, 0, 7);
        ImGui::TableHeadersRow();

        if (auto* specs = ImGui::TableGetSortSpecs()) {
            if (specs->SpecsDirty && specs->SpecsCount > 0) {
                sortCol_ = specs->Specs[0].ColumnUserID;
                sortAsc_ = (specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                specs->SpecsDirty = false;
                std::sort(filtered.begin(), filtered.end(), cmp);
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filtered.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                auto* p = filtered[i];
                ImGui::TableNextRow();

                bool selected = (p->pid == selectedPid_);
                ImGui::TableNextColumn();
                char pidLbl[32]; snprintf(pidLbl, 32, "%d", p->pid);
                if (ImGui::Selectable(pidLbl, selected,
                        ImGuiSelectableFlags_SpanAllColumns))
                    selectedPid_ = p->pid;

                ImGui::TableNextColumn(); ImGui::Text("%s", p->name.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%c", p->state);
                char mb[32];
                ImGui::TableNextColumn(); ImGui::Text("%s", Theme::FormatBytes(p->memoryBytes, mb, 32));
                ImGui::TableNextColumn();
                ImGui::TextColored(Theme::SeverityColor(p->cpuPercent),
                                   "%.1f", p->cpuPercent);
                ImGui::TableNextColumn(); ImGui::Text("%d", p->threads);
                ImGui::TableNextColumn(); ImGui::Text("%d", p->priority);
                ImGui::TableNextColumn(); ImGui::Text("%s", p->user.c_str());
            }
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
//  ALERTS TAB
// ---------------------------------------------------------------------------

inline void App::renderAlertTab() {
    auto rules = alerts_.getRules();
    auto events = alerts_.getEvents();

    ImGui::TextColored(Theme::TextPrimary, "Alert Rules (%d)", (int)rules.size());

    ImGui::InputTextWithHint("##aname", "Rule name", newAlertName_, 64);
    ImGui::SameLine();
    const char* metricNames[] = {
        "CPU Usage","Memory Usage","Swap Usage","Disk Usage",
        "GPU Usage","CPU Temp","GPU Temp","Net Upload","Net Download"
    };
    ImGui::Combo("Metric", &newAlertMetric_, metricNames, 9);
    ImGui::SliderFloat("Threshold", &newAlertThresh_, 0, 100, "%.0f");
    ImGui::Checkbox("Above", &newAlertAbove_);
    ImGui::SameLine();
    ImGui::SliderInt("Sustain (s)", &newAlertSustain_, 1, 60);
    ImGui::SameLine();
    if (ImGui::Button("Add Rule") && newAlertName_[0]) {
        AlertRule r;
        r.name = newAlertName_;
        r.metric = static_cast<AlertMetric>(newAlertMetric_);
        r.threshold = newAlertThresh_;
        r.above = newAlertAbove_;
        r.sustainSeconds = newAlertSustain_;
        alerts_.addRule(r);
        newAlertName_[0] = '\0';
    }

    ImGui::Separator();

    if (!rules.empty() &&
        ImGui::BeginTable("##rules", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("Threshold");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (auto& r : rules) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", r.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", metricNames[static_cast<int>(r.metric)]);
            ImGui::TableNextColumn(); ImGui::Text("%.0f", r.threshold);
            ImGui::TableNextColumn();
            ImGui::TextColored(r.triggered ? Theme::AccentRed : Theme::AccentGreen,
                               "%s", r.triggered ? "TRIGGERED" : "OK");
            ImGui::TableNextColumn(); ImGui::Text("%.1f", r.currentValue);
            ImGui::TableNextColumn();
            char delBtn[32]; snprintf(delBtn, 32, "Del##%d", r.id);
            if (ImGui::SmallButton(delBtn)) alerts_.removeRule(r.id);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextColored(Theme::TextPrimary, "Recent Events (%d)", (int)events.size());
    if (!events.empty() &&
        ImGui::BeginTable("##events", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Rule");
        ImGui::TableSetupColumn("Message");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", it->timestamp.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", it->ruleName.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", it->message.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.1f / %.0f", it->value, it->threshold);
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
//  SYSTEM INFO TAB — with data export controls
// ---------------------------------------------------------------------------

inline void App::renderSystemTab() {
    MetricData d;
    { std::lock_guard<std::recursive_mutex> lk(dataMtx_); d = latest_; }
    auto& s = d.systemInfo;

    auto row = [](const char* label, const char* value) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(Theme::TextSecondary, "%s", label);
        ImGui::TableNextColumn(); ImGui::TextColored(Theme::TextPrimary, "%s", value);
    };

    if (ImGui::BeginTable("##sysinfo", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(600, 0))) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        row("OS",           s.osName.c_str());
        row("OS Version",   s.osVersion.c_str());
        row("Kernel",       s.kernelVersion.c_str());
        row("Hostname",     s.hostname.c_str());
        row("Architecture", s.arch.c_str());
        row("CPU Model",    s.cpuModel.c_str());

        char cores[64]; snprintf(cores, 64, "%d physical / %d logical",
                                 s.cpuPhysicalCores, s.cpuLogicalCores);
        row("CPU Cores", cores);

        if (s.l1CacheKB > 0) {
            char cache[128];
            snprintf(cache, 128, "L1: %u KB  |  L2: %u KB  |  L3: %u KB",
                     s.l1CacheKB, s.l2CacheKB, s.l3CacheKB);
            row("CPU Cache", cache);
        }

        char ram[32]; Theme::FormatBytes(s.totalRAM, ram, 32);
        row("Total RAM", ram);
        row("GPU", s.gpuModel.c_str());

        uint64_t up = s.uptimeSeconds;
        char upBuf[64]; snprintf(upBuf, 64, "%llud %lluh %llum %llus",
            (unsigned long long)(up/86400), (unsigned long long)((up%86400)/3600),
            (unsigned long long)((up%3600)/60), (unsigned long long)(up%60));
        row("Uptime", upBuf);

        ImGui::EndTable();
    }

    // ---- Data Export Section ----
    ImGui::Separator();
    ImGui::TextColored(Theme::TextPrimary, "Data Export");

    const char* timeframes[] = {"Last Hour", "Last 24 Hours", "Last 7 Days", "Last 30 Days"};
    ImGui::Combo("Timeframe", &exportTimeframe_, timeframes, 4);

    ImGui::TextColored(Theme::TextSecondary, "Data types to export:");
    ImGui::Checkbox("CPU", &exportCpu_); ImGui::SameLine();
    ImGui::Checkbox("Memory", &exportMem_); ImGui::SameLine();
    ImGui::Checkbox("Network", &exportNet_); ImGui::SameLine();
    ImGui::Checkbox("Disk", &exportDisk_); ImGui::SameLine();
    ImGui::Checkbox("GPU", &exportGpu_);

    const char* formats[] = {"CSV", "TXT (tab-separated)"};
    ImGui::Combo("Format", &exportFormat_, formats, 2);

    if (ImGui::Button("Export Data")) {
        int hours = 1;
        switch (exportTimeframe_) {
            case 0: hours = 1; break;
            case 1: hours = 24; break;
            case 2: hours = 168; break;
            case 3: hours = 720; break;
        }
        db_.exportFiltered(".", hours,
            exportCpu_, exportMem_, exportNet_, exportDisk_, exportGpu_,
            exportFormat_ == 0);
        snprintf(exportStatus_, sizeof(exportStatus_),
            "Exported %s data (%s) for last %s",
            exportFormat_ == 0 ? "CSV" : "TXT",
            "selected types", timeframes[exportTimeframe_]);
    }

    if (exportStatus_[0]) {
        ImGui::SameLine();
        ImGui::TextColored(Theme::AccentGreen, "%s", exportStatus_);
    }
}
