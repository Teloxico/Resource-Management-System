// File: src/core/memory/memory_linux.cpp

#ifdef __linux__

#include "memory_linux.h"
#include "utils/logger.h" // Ensure Logger is correctly implemented.
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <sys/sysinfo.h>

/**
 * @brief Constructs a new LinuxMemory object and initializes memory statistics.
 */
LinuxMemory::LinuxMemory()
    : mem_total_(0), mem_available_(0), mem_free_(0), mem_cached_(0), mem_buffers_(0),
      max_history_size_(100), last_process_update_(std::chrono::steady_clock::now()),
      process_update_interval_(std::chrono::seconds(5)),
      running_(true) {
    if (readMemInfo()) {
        updateUsageHistory();
    } else {
        Logger::log("Failed to initialize memory stats.");
    }
    usage_history_.reserve(max_history_size_);
    update_thread_ = std::thread(&LinuxMemory::updateLoop, this); // Start the update thread
}

/**
 * @brief Destructs the LinuxMemory object.
 *
 * Ensures that all resources are properly released.
 */
LinuxMemory::~LinuxMemory() {
    running_ = false;
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}

/**
 * @brief The loop function run by the update thread to periodically refresh memory metrics.
 */
void LinuxMemory::updateLoop() {
    while (running_) {
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (readMemInfo()) {
                updateUsageHistory();
            }
            readProcessMemoryUsage();
        }
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Update every second
    }
}

/**
 * @brief Reads memory information from /proc/meminfo and updates member variables.
 *
 * @return true If memory information was successfully read.
 * @return false If there was an error reading memory information.
 */
bool LinuxMemory::readMemInfo() {
    std::ifstream meminfo_file(PROC_MEMINFO);
    if (!meminfo_file.is_open()) {
        Logger::log("Failed to open /proc/meminfo for reading memory statistics.");
        return false;
    }

    std::string line;
    // Reset variables before reading
    mem_total_ = mem_available_ = mem_free_ = mem_cached_ = mem_buffers_ = 0;

    while (std::getline(meminfo_file, line)) {
        std::istringstream ss(line);
        std::string key;
        unsigned long value;
        ss >> key >> value;

        if (key == "MemTotal:") mem_total_ = value;
        else if (key == "MemAvailable:") mem_available_ = value;
        else if (key == "MemFree:") mem_free_ = value;
        else if (key == "Cached:") mem_cached_ = value;
        else if (key == "Buffers:") mem_buffers_ = value;
    }

    meminfo_file.close();

    Logger::log("MemTotal: " + std::to_string(mem_total_) +
               ", MemAvailable: " + std::to_string(mem_available_) +
               ", MemFree: " + std::to_string(mem_free_) +
               ", MemCached: " + std::to_string(mem_cached_) +
               ", MemBuffers: " + std::to_string(mem_buffers_));

    // Validate that essential fields were read
    if (mem_total_ == 0 || mem_available_ == 0) {
        Logger::log("Incomplete memory information retrieved.");
        return false;
    }

    return true;
}

/**
 * @brief Updates the history of memory usage percentages.
 */
void LinuxMemory::updateUsageHistory() {
    float usage = (static_cast<float>(mem_total_ - mem_available_) / mem_total_) * 100.0f;
    usage_history_.push_back(usage);
    if (usage_history_.size() > max_history_size_) {
        usage_history_.erase(usage_history_.begin());
    }
    Logger::log("Updated usage history: " + std::to_string(usage) + "%");
}

/**
 * @brief Retrieves the latest total memory usage percentage.
 *
 * @return float Total memory usage as a percentage.
 */
float LinuxMemory::getTotalUsage() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    float latest_usage = 0.0f;
    if (readMemInfo()) {
        updateUsageHistory();
        latest_usage = usage_history_.back();
        Logger::log("Total Memory Usage: " + std::to_string(latest_usage) + "%");
    } else {
        Logger::log("Failed to read memory info for total usage.");
    }
    return latest_usage;
}

/**
 * @brief Retrieves the remaining RAM in megabytes.
 *
 * @return float Remaining RAM in MB.
 */
float LinuxMemory::getRemainingRAM() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    float remaining_ram = 0.0f;
    if (readMemInfo()) {
        remaining_ram = static_cast<float>(mem_available_) / 1024.0f; // Convert KB to MB
        Logger::log("Remaining RAM: " + std::to_string(remaining_ram) + " MB");
    } else {
        Logger::log("Failed to read memory info for remaining RAM.");
    }
    return remaining_ram;
}

/**
 * @brief Retrieves the average memory usage percentage over the history.
 *
 * @return float Average memory usage as a percentage.
 */
float LinuxMemory::getAverageUsage() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    float average = 0.0f;
    if (!usage_history_.empty()) {
        float sum = std::accumulate(usage_history_.begin(), usage_history_.end(), 0.0f);
        average = sum / static_cast<float>(usage_history_.size());
        Logger::log("Average Memory Usage: " + std::to_string(average) + "%");
    } else {
        Logger::log("Usage history is empty. Cannot determine average Memory usage.");
    }
    return average;
}

/**
 * @brief Reads process memory usage from /proc/[PID]/status and identifies the top memory-consuming process.
 *
 * @return true If process memory usage was successfully read.
 * @return false If there was an error reading process memory usage.
 */
bool LinuxMemory::readProcessMemoryUsage() {
    auto now = std::chrono::steady_clock::now();
    if (now - last_process_update_ < process_update_interval_) {
        Logger::log("Using cached process memory usage data.");
        return true; // Use cached data if it's recent enough
    }

    process_memory_usage_.clear();
    DIR* proc_dir = opendir(PROC_DIR);
    if (!proc_dir) {
        Logger::log("Failed to open /proc directory for identifying top memory process.");
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;

        std::string pid_str(entry->d_name);
        if (!std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) continue;

        std::string status_path = std::string(PROC_DIR) + "/" + pid_str + "/status";
        std::ifstream status_file(status_path);
        if (!status_file.is_open()) continue;

        std::string line, process_name;
        unsigned long vmrss = 0;

        while (std::getline(status_file, line)) {
            if (line.compare(0, 5, "Name:") == 0) {
                process_name = line.substr(6);
                process_name.erase(0, process_name.find_first_not_of(" \t"));
            } else if (line.compare(0, 6, "VmRSS:") == 0) {
                std::istringstream ss(line);
                std::string key;
                ss >> key >> vmrss;
                break;
            }
        }

        if (!process_name.empty() && vmrss > 0) {
            process_memory_usage_[process_name] = vmrss;
            Logger::log("Process: " + process_name + ", VmRSS: " + std::to_string(vmrss) + " kB");
        }
    }

    closedir(proc_dir);
    last_process_update_ = now;
    Logger::log("Completed reading process memory usage.");
    return !process_memory_usage_.empty();
}

/**
 * @brief Retrieves the name of the process consuming the most memory.
 *
 * @return std::string Name of the top memory-consuming process.
 */
std::string LinuxMemory::getMostUsingProcess() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::string top_process = "N/A";
    if (readProcessMemoryUsage()) {
        // Find the process with the maximum VmRSS
        auto max_it = std::max_element(process_memory_usage_.begin(), process_memory_usage_.end(),
            [](const auto& p1, const auto& p2) { return p1.second < p2.second; });

        if (max_it != process_memory_usage_.end()) {
            float vmrss_mb = static_cast<float>(max_it->second) / 1024.0f; // Convert kB to MB
            top_process = max_it->first + " (" + std::to_string(vmrss_mb) + " MB)";
            Logger::log("Top Memory Process: " + top_process);
        } else {
            Logger::log("No processes found consuming memory.");
        }
    } else {
        Logger::log("Failed to read process memory usage.");
    }
    return top_process;
}

#endif // __linux__

