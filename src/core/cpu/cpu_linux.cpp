// File: src/core/cpu/cpu_linux.cpp

#ifdef __linux__

#include "cpu_linux.h"
#include "utils/logger.h"
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <chrono>
#include <thread>
#include <dirent.h>
#include <algorithm>
#include <numeric>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <memory>
#include <cctype>

/**
 * @class DirGuard
 * @brief RAII class to manage DIR* resource.
 *
 * Ensures that the directory stream is closed when the object goes out of scope.
 */
class DirGuard {
public:
    /**
     * @brief Constructs a DirGuard object with a DIR*.
     * @param dir Pointer to a directory stream.
     */
    explicit DirGuard(DIR* dir) : dir_(dir) {}

    /**
     * @brief Destructor that closes the directory stream if it's open.
     */
    ~DirGuard() {
        if (dir_) {
            closedir(dir_);
        }
    }

    // Delete copy constructor and assignment
    DirGuard(const DirGuard&) = delete;
    DirGuard& operator=(const DirGuard&) = delete;

    // Allow move semantics
    DirGuard(DirGuard&& other) noexcept : dir_(other.dir_) {
        other.dir_ = nullptr;
    }

    DirGuard& operator=(DirGuard&& other) noexcept {
        if (this != &other) {
            if (dir_) {
                closedir(dir_);
            }
            dir_ = other.dir_;
            other.dir_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Retrieves the DIR*.
     * @return Pointer to the directory stream.
     */
    DIR* get() const { return dir_; }

private:
    DIR* dir_; ///< Pointer to the directory stream.
};

// Constructor
LinuxCPU::LinuxCPU() : prev_total_(0), prev_idle_(0), max_history_size_(100) {
    // Initial read to populate prev_total_ and prev_idle_
    std::vector<unsigned long long> stats;
    if (readCPUStat(stats) && stats.size() >= 5) {
        prev_idle_ = stats[3] + stats[4]; // idle + iowait
        prev_total_ = std::accumulate(stats.begin(), stats.end(), 0ULL);
    } else {
        Logger::log("Insufficient CPU stats retrieved during initialization.");
    }

    // Initialize usage history
    usage_history_.reserve(max_history_size_);
}

// Destructor
LinuxCPU::~LinuxCPU() = default;

bool LinuxCPU::readCPUStat(std::vector<unsigned long long>& stats) {
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        Logger::log("Failed to open /proc/stat for reading CPU statistics.");
        return false;
    }

    std::string line;
    std::getline(stat_file, line);
    std::istringstream ss(line);

    std::string cpu_label;
    unsigned long long value;
    ss >> cpu_label;
    while (ss >> value) {
        stats.push_back(value);
    }

    if (stats.empty()) {
        Logger::log("No CPU stats found in /proc/stat.");
        return false;
    }

    return true;
}

float LinuxCPU::getTotalUsage() {
    std::vector<unsigned long long> stats;
    if (!readCPUStat(stats) || stats.size() < 5) {
        Logger::log("Insufficient CPU stats retrieved for usage calculation.");
        return 0.0f;
    }

    unsigned long long idle = stats[3] + stats[4]; // idle + iowait
    unsigned long long total = std::accumulate(stats.begin(), stats.end(), 0ULL);

    unsigned long long delta_total = total - prev_total_;
    unsigned long long delta_idle = idle - prev_idle_;

    prev_total_ = total;
    prev_idle_ = idle;

    if (delta_total == 0) {
        Logger::log("Delta total CPU time is zero. Skipping usage calculation.");
        return 0.0f;
    }

    float usage = (static_cast<float>(delta_total - delta_idle) / delta_total) * 100.0f;

    // Update usage history
    usage_history_.push_back(usage);
    if (usage_history_.size() > max_history_size_) {
        usage_history_.erase(usage_history_.begin());
    }

    return usage;
}

float LinuxCPU::getClockFrequency() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo.is_open()) {
        Logger::log("Failed to open /proc/cpuinfo for reading CPU frequency.");
        return 0.0f;
    }

    std::string line;
    float mhz = 0.0f;

    while (std::getline(cpuinfo, line)) {
        if (line.find("cpu MHz") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string freq_str = line.substr(pos + 1);
                mhz = std::stof(freq_str);
                break;
            }
        }
    }

    if (mhz == 0.0f) {
        Logger::log("Failed to retrieve CPU clock frequency from /proc/cpuinfo.");
    }

    return mhz / 1000.0f; // Convert MHz to GHz
}

int LinuxCPU::getUsedThreads() {
    const char* task_path = "/proc/self/task";
    DIR* dir = opendir(task_path);
    if (!dir) {
        Logger::log("Error: Failed to open " + std::string(task_path) + " for thread counting. Error: " + std::strerror(errno));
        return 0;
    }

    // RAII Wrapper ensures the directory is closed when out of scope
    DirGuard dir_guard(dir);

    int thread_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check if the entry is a directory or if d_type is unknown
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) {
            continue;
        }

        std::string thread_id(entry->d_name);

        // Validate that the thread ID consists solely of digits
        if (!thread_id.empty() && std::all_of(thread_id.begin(), thread_id.end(), ::isdigit)) {
            thread_count++;
        } else {
            Logger::log("Warning: Unexpected entry in " + std::string(task_path) + ": " + thread_id);
        }
    }

    Logger::log("Current Process Threads: " + std::to_string(thread_count));
    return thread_count;
}

int LinuxCPU::getTotalThreads() {
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
}

float LinuxCPU::getHighestUsage() {
    if (usage_history_.empty()) {
        Logger::log("Usage history is empty. Cannot determine highest CPU usage.");
        return 0.0f;
    }
    return *std::max_element(usage_history_.begin(), usage_history_.end());
}

float LinuxCPU::getAverageUsage() {
    if (usage_history_.empty()) {
        Logger::log("Usage history is empty. Cannot determine average CPU usage.");
        return 0.0f;
    }
    float sum = std::accumulate(usage_history_.begin(), usage_history_.end(), 0.0f);
    return sum / usage_history_.size();
}

#endif // __linux__

