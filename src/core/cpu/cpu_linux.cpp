/**
 * @file cpu_linux.cpp
 * @brief Linux CPU monitoring using procfs, sysfs, and hwmon.
 */

#ifdef __linux__

#include "cpu_linux.h"

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <filesystem>

namespace fs = std::filesystem;

LinuxCPU::LinuxCPU()
    : prevTime_(std::chrono::steady_clock::now())
{
    logicalCores_ = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (logicalCores_ < 1) logicalCores_ = 1;

    prevCores_.resize(logicalCores_);
    usageHistory_.reserve(kMaxHistory);

    std::ifstream stat("/proc/stat");
    if (stat.is_open()) {
        std::string line;
        while (std::getline(stat, line)) {
            if (line.compare(0, 4, "cpu ") == 0) {
                std::istringstream ss(line.substr(4));
                ss >> prevAgg_.user >> prevAgg_.nice >> prevAgg_.system
                   >> prevAgg_.idle >> prevAgg_.iowait >> prevAgg_.irq
                   >> prevAgg_.softirq >> prevAgg_.steal;
            } else if (line.compare(0, 3, "cpu") == 0 && std::isdigit(line[3])) {
                int idx = 0;
                std::istringstream ss(line.substr(3));
                ss >> idx;
                if (idx >= 0 && idx < logicalCores_) {
                    auto& c = prevCores_[idx];
                    ss >> c.user >> c.nice >> c.system >> c.idle
                       >> c.iowait >> c.irq >> c.softirq >> c.steal;
                }
            } else if (line.compare(0, 4, "ctxt") == 0) {
                std::istringstream ss(line.substr(4));
                ss >> prevCtx_;
            } else if (line.compare(0, 4, "intr") == 0) {
                std::istringstream ss(line.substr(4));
                ss >> prevIntr_;
            }
        }
    }
}

float LinuxCPU::computeUsage(const CoreTick& prev, const CoreTick& cur) {
    uint64_t dTotal  = cur.total() - prev.total();
    uint64_t dActive = cur.activeTime() - prev.activeTime();
    if (dTotal == 0) return 0.0f;
    return static_cast<float>(dActive) * 100.0f / static_cast<float>(dTotal);
}

/**
 * @brief Read CPU temperature from /sys/class/hwmon, trying known drivers first.
 * @return Temperature in Celsius, or -1 on failure.
 */
float LinuxCPU::readTemperature() const {
    static const char* preferredDrivers[] = {
        "coretemp", "k10temp", "zenpower",
        "it87", "nct6775", "nct6776", "nct6779",
        "thinkpad", "acpitz"
    };

    try {
        for (const char* wanted : preferredDrivers) {
            for (const auto& hwmon : fs::directory_iterator("/sys/class/hwmon")) {
                std::ifstream nameFile(hwmon.path() / "name");
                if (!nameFile.is_open()) continue;

                std::string name;
                std::getline(nameFile, name);
                while (!name.empty() && (name.back() == '\n' || name.back() == ' '))
                    name.pop_back();

                if (name != wanted) continue;

                for (int idx = 1; idx <= 4; ++idx) {
                    std::string tempPath = (hwmon.path() / ("temp" + std::to_string(idx) + "_input")).string();
                    std::ifstream tempFile(tempPath);
                    if (tempFile.is_open()) {
                        int millideg = 0;
                        tempFile >> millideg;
                        if (!tempFile.fail() && millideg > 0)
                            return static_cast<float>(millideg) / 1000.0f;
                    }
                }
            }
        }

        for (const auto& hwmon : fs::directory_iterator("/sys/class/hwmon")) {
            std::ifstream tempFile(hwmon.path() / "temp1_input");
            if (tempFile.is_open()) {
                int millideg = 0;
                tempFile >> millideg;
                if (!tempFile.fail() && millideg > 0)
                    return static_cast<float>(millideg) / 1000.0f;
            }
        }
    } catch (...) {
    }
    return -1.0f;
}

void LinuxCPU::update() {
    CpuSnapshot snap;
    snap.logicalCores  = logicalCores_;
    snap.physicalCores = logicalCores_;

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - prevTime_).count();
    if (elapsed <= 0.0) elapsed = 1.0;

    CoreTick aggNow{};
    std::vector<CoreTick> coresNow(logicalCores_);
    uint64_t ctxNow  = 0;
    uint64_t intrNow = 0;

    {
        std::ifstream stat("/proc/stat");
        if (stat.is_open()) {
            std::string line;
            while (std::getline(stat, line)) {
                if (line.compare(0, 4, "cpu ") == 0) {
                    std::istringstream ss(line.substr(4));
                    ss >> aggNow.user >> aggNow.nice >> aggNow.system
                       >> aggNow.idle >> aggNow.iowait >> aggNow.irq
                       >> aggNow.softirq >> aggNow.steal;
                } else if (line.compare(0, 3, "cpu") == 0 && std::isdigit(line[3])) {
                    int idx = 0;
                    std::istringstream ss(line.substr(3));
                    ss >> idx;
                    if (idx >= 0 && idx < logicalCores_) {
                        auto& c = coresNow[idx];
                        ss >> c.user >> c.nice >> c.system >> c.idle
                           >> c.iowait >> c.irq >> c.softirq >> c.steal;
                    }
                } else if (line.compare(0, 4, "ctxt") == 0) {
                    std::istringstream ss(line.substr(4));
                    ss >> ctxNow;
                } else if (line.compare(0, 4, "intr") == 0) {
                    std::istringstream ss(line.substr(4));
                    ss >> intrNow;
                }
            }
        }
    }

    {
        uint64_t dTotal = aggNow.total() - prevAgg_.total();
        if (dTotal > 0) {
            auto pct = [&](uint64_t cur, uint64_t prev) {
                return static_cast<float>(cur - prev) * 100.0f / static_cast<float>(dTotal);
            };
            snap.userPercent   = pct(aggNow.user + aggNow.nice, prevAgg_.user + prevAgg_.nice);
            snap.systemPercent = pct(aggNow.system + aggNow.irq + aggNow.softirq,
                                     prevAgg_.system + prevAgg_.irq + prevAgg_.softirq);
            snap.idlePercent   = pct(aggNow.idle, prevAgg_.idle);
            snap.iowaitPercent = pct(aggNow.iowait, prevAgg_.iowait);
            snap.totalUsage    = 100.0f - snap.idlePercent - snap.iowaitPercent;
            if (snap.totalUsage < 0.0f) snap.totalUsage = 0.0f;
        }
    }

    snap.cores.resize(logicalCores_);
    for (int i = 0; i < logicalCores_; ++i) {
        snap.cores[i].id    = i;
        snap.cores[i].usage = computeUsage(prevCores_[i], coresNow[i]);
        snap.cores[i].temperature = -1.0f;
    }

    if (elapsed > 0.0) {
        snap.contextSwitchesPerSec = static_cast<float>(
            static_cast<double>(ctxNow - prevCtx_) / elapsed);
        snap.interruptsPerSec = static_cast<float>(
            static_cast<double>(intrNow - prevIntr_) / elapsed);
    }

    prevAgg_   = aggNow;
    prevCores_ = coresNow;
    prevCtx_   = ctxNow;
    prevIntr_  = intrNow;
    prevTime_  = now;

    {
        float freqSum   = 0.0f;
        int   freqCount = 0;
        bool  sysfsOk   = false;

        for (int i = 0; i < logicalCores_; ++i) {
            std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(i)
                             + "/cpufreq/scaling_cur_freq";
            std::ifstream f(path);
            if (f.is_open()) {
                int khz = 0;
                f >> khz;
                if (!f.fail() && khz > 0) {
                    float mhz = static_cast<float>(khz) / 1000.0f;
                    if (i < logicalCores_)
                        snap.cores[i].frequency = mhz;
                    freqSum += mhz;
                    ++freqCount;
                    sysfsOk = true;
                }
            }
        }

        if (freqCount > 0)
            snap.frequency = freqSum / static_cast<float>(freqCount);

        if (!sysfsOk) {
            std::ifstream cpuinfo("/proc/cpuinfo");
            if (cpuinfo.is_open()) {
                std::string line;
                float cFreqSum = 0.0f;
                int   cFreqCount = 0;
                int   coreIdx = 0;
                while (std::getline(cpuinfo, line)) {
                    if (line.compare(0, 7, "cpu MHz") == 0) {
                        auto pos = line.find(':');
                        if (pos != std::string::npos) {
                            float mhz = 0.0f;
                            try { mhz = std::stof(line.substr(pos + 1)); } catch (...) {}
                            cFreqSum += mhz;
                            ++cFreqCount;
                            if (coreIdx < logicalCores_)
                                snap.cores[coreIdx].frequency = mhz;
                            ++coreIdx;
                        }
                    }
                }
                if (cFreqCount > 0)
                    snap.frequency = cFreqSum / static_cast<float>(cFreqCount);
            }
        }
    }

    {
        std::ifstream cpuinfo("/proc/cpuinfo");
        if (cpuinfo.is_open()) {
            std::string line;
            while (std::getline(cpuinfo, line)) {
                if (line.compare(0, 9, "cpu cores") == 0) {
                    auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        try {
                            int c = std::stoi(line.substr(pos + 1));
                            if (c > 0) snap.physicalCores = c;
                        } catch (...) {}
                    }
                    break;
                }
            }
        }
        if (snap.physicalCores == 0 || snap.physicalCores > logicalCores_)
            snap.physicalCores = logicalCores_;
    }

    {
        std::ifstream la("/proc/loadavg");
        if (la.is_open()) {
            la >> snap.loadAvg1 >> snap.loadAvg5 >> snap.loadAvg15;
        }
    }

    {
        int count = 0;
        try {
            for (const auto& entry : fs::directory_iterator("/proc/self/task")) {
                (void)entry;
                ++count;
            }
        } catch (...) {}
        snap.processThreads = count;
    }

    {
        std::ifstream la("/proc/loadavg");
        if (la.is_open()) {
            float f1, f2, f3;
            std::string runTotal;
            la >> f1 >> f2 >> f3 >> runTotal;
            auto slash = runTotal.find('/');
            if (slash != std::string::npos) {
                try {
                    snap.totalThreads = std::stoi(runTotal.substr(slash + 1));
                } catch (...) {}
            }
        }
    }

    snap.temperature = readTemperature();

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

CpuSnapshot LinuxCPU::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

#endif // __linux__
