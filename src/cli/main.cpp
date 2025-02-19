// File: src/cli/main.cpp

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <iomanip>
#include <numeric>
#include <string>
#include "core/cpu/cpu_common.h"
#include "core/memory/memory_common.h"
#include "core/network/network_common.h"
#include "core/database/database.h"
#include "utils/logger.h"

// Global atomic flag for termination
std::atomic<bool> keep_running(true);

// Variables to store highest upload and download rates
float highest_upload_rate = 0.0f;
float highest_download_rate = 0.0f;

// Declare pointers to CPU, Memory, Network, and Database
CPU* cpu = nullptr;
Memory* memory = nullptr;
Network* network = nullptr;
Database* db = nullptr;

/**
 * @brief Signal handler for graceful termination.
 * @param signal Signal number received.
 */
void signal_handler(int signal) {
    if (signal == SIGINT) {
        keep_running = false;
    }
}

/**
 * @brief Clears the console screen.
 */
void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif
}

/**
 * @brief Centers text within a given width.
 * @param text The text to center.
 * @param width The width to center within.
 * @return A string with the text centered.
 */
std::string centerText(const std::string& text, int width) {
    int padding = (width - static_cast<int>(text.length())) / 2;
    if (padding > 0) {
        return std::string(padding, ' ') + text + std::string(padding + (text.length() % 2 == 0 ? 0 : 1), ' ');
    } else {
        return text;
    }
}

/**
 * @brief Entry point for the CLI application.
 * @return int Exit code.
 */
int main() {
    // Initialize Logger
    Logger::initialize("ResourceMonitor.log");

    // Register signal handler for graceful termination
    signal(SIGINT, signal_handler);

    // Initialize Database
    db = new Database("resource_monitor.db");
    db->initialize();

    // Create instances using factory functions
    cpu = createCPU();
    memory = createMemory();
    network = createNetwork();

    // Verify that modules are initialized successfully
    if (!cpu || !memory || !network) {
        std::cerr << "Failed to initialize monitoring modules." << std::endl;
        Logger::log("Failed to initialize monitoring modules.");
        delete cpu;
        delete memory;
        delete network;
        delete db;
        return EXIT_FAILURE;
    }

    std::cout << "Monitoring resources..." << std::endl;
    Logger::log("CLI started.");

    // Monitoring loop: Run until interrupted
    while (keep_running) {
        // Retrieve CPU metrics
        float cpu_usage = cpu->getTotalUsage();
        float cpu_freq = cpu->getClockFrequency();
        int used_threads = cpu->getUsedThreads();
        int total_threads = cpu->getTotalThreads();
        float highest_cpu = cpu->getHighestUsage();
        float average_cpu = cpu->getAverageUsage();

        // Retrieve Memory metrics
        float mem_usage = memory->getTotalUsage();
        float remaining_ram = memory->getRemainingRAM();
        float average_mem_usage = memory->getAverageUsage();
        std::string top_mem_process = memory->getMostUsingProcess();

        // Calculate Used RAM (MB)
        float total_ram = remaining_ram / ((100.0f - mem_usage) / 100.0f);
        float used_ram = total_ram - remaining_ram;

        // Retrieve Network metrics
        float upload_rate = network->getUploadRate();
        float download_rate = network->getDownloadRate();
        float total_bandwidth_used = network->getTotalUsedBandwidth();

        // Update highest upload and download rates
        if (upload_rate > highest_upload_rate) {
            highest_upload_rate = upload_rate;
        }
        if (download_rate > highest_download_rate) {
            highest_download_rate = download_rate;
        }

        // Insert data into database
        db->insertCPUData(cpu_usage, cpu_freq, used_threads, total_threads, highest_cpu, average_cpu);
        db->insertMemoryData(mem_usage, remaining_ram, average_mem_usage, top_mem_process);
        db->insertNetworkData(upload_rate, download_rate, total_bandwidth_used);

        // Clear the console
        clearConsole();

        // Width for the entire table
        int table_width = 110;

        // Print CPU Metrics
        std::cout << std::string(table_width, '-') << std::endl;
        std::cout << "|" << centerText("CPU", table_width - 2) << "|" << std::endl;
        std::cout << std::string(table_width, '-') << std::endl;
        std::cout << "| Total Usage               : " << std::setw(8) << std::fixed << std::setprecision(2) << cpu_usage << "%" << std::string(table_width - 40, ' ') << "|" << std::endl;
        std::cout << "| Clock Base Frequency      : " << std::setw(8) << std::fixed << std::setprecision(2) << cpu_freq << " GHz" << std::string(table_width - 43, ' ') << "|" << std::endl;
        std::cout << "| Used Cores                : " << std::setw(8) << used_threads << std::string(table_width - 39, ' ') << "|" << std::endl;
        std::cout << "| Total Threads             : " << std::setw(8) << total_threads << std::string(table_width - 39, ' ') << "|" << std::endl;
        std::cout << "| Highest Usage             : " << std::setw(8) << std::fixed << std::setprecision(2) << highest_cpu << "%" << std::string(table_width - 40, ' ') << "|" << std::endl;
        std::cout << "| Average Usage             : " << std::setw(8) << std::fixed << std::setprecision(2) << average_cpu << "%" << std::string(table_width - 40, ' ') << "|" << std::endl;
        std::cout << std::string(table_width, '-') << std::endl;

        // Print Memory Metrics
        std::cout << "|" << centerText("MEMORY", table_width - 2) << "|" << std::endl;
        std::cout << std::string(table_width, '-') << std::endl;
        std::cout << "| Total Usage               : " << std::setw(8) << std::fixed << std::setprecision(2) << mem_usage << "%" << std::string(table_width - 40, ' ') << "|" << std::endl;
        std::cout << "| Used RAM                  : " << std::setw(8) << std::fixed << std::setprecision(2) << used_ram << " MB" << std::string(table_width - 42, ' ') << "|" << std::endl;
        std::cout << "| Remaining RAM             : " << std::setw(8) << std::fixed << std::setprecision(2) << remaining_ram << " MB" << std::string(table_width - 42, ' ') << "|" << std::endl;
        std::cout << "| Average Usage             : " << std::setw(8) << std::fixed << std::setprecision(2) << average_mem_usage << "%" << std::string(table_width - 40, ' ') << "|" << std::endl;
        std::cout << "| Top Memory Process        : " << std::setw(20) << top_mem_process << std::string(table_width - 51, ' ') << "|" << std::endl;
        std::cout << std::string(table_width, '-') << std::endl;

        // Print Network Metrics
        std::cout << "|" << centerText("NETWORK", table_width - 2) << "|" << std::endl;
        std::cout << std::string(table_width, '-') << std::endl;
        std::cout << "| Upload Rate               : " << std::setw(8) << std::fixed << std::setprecision(2) << upload_rate << " MB/s" << std::string(table_width - 44, ' ') << "|" << std::endl;
        std::cout << "| Download Rate             : " << std::setw(8) << std::fixed << std::setprecision(2) << download_rate << " MB/s" << std::string(table_width - 44, ' ') << "|" << std::endl;
        std::cout << "| Total Used Bandwidth      : " << std::setw(8) << std::fixed << std::setprecision(2) << total_bandwidth_used << " Mbps" << std::string(table_width - 45, ' ') << "|" << std::endl;
        std::cout << "| Highest Upload Rate       : " << std::setw(8) << std::fixed << std::setprecision(2) << highest_upload_rate << " MB/s" << std::string(table_width - 44, ' ') << "|" << std::endl;
        std::cout << "| Highest Download Rate     : " << std::setw(8) << std::fixed << std::setprecision(2) << highest_download_rate << " MB/s" << std::string(table_width - 44, ' ') << "|" << std::endl;
        std::cout << std::string(table_width, '-') << std::endl;

        // Wait for 1 second before updating again
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // After loop exits, collect data one more time and save to database
    // Retrieve CPU metrics
    float cpu_usage = cpu->getTotalUsage();
    float cpu_freq = cpu->getClockFrequency();
    int used_threads = cpu->getUsedThreads();
    int total_threads = cpu->getTotalThreads();
    float highest_cpu = cpu->getHighestUsage();
    float average_cpu = cpu->getAverageUsage();

    // Retrieve Memory metrics
    float mem_usage = memory->getTotalUsage();
    float remaining_ram = memory->getRemainingRAM();
    float average_mem_usage = memory->getAverageUsage();
    std::string top_mem_process = memory->getMostUsingProcess();

    // Calculate Used RAM (MB)
    float total_ram = remaining_ram / ((100.0f - mem_usage) / 100.0f);
    float used_ram = total_ram - remaining_ram;

    // Retrieve Network metrics
    float upload_rate = network->getUploadRate();
    float download_rate = network->getDownloadRate();
    float total_bandwidth_used = network->getTotalUsedBandwidth();

    // Update highest upload and download rates if necessary
    if (upload_rate > highest_upload_rate) {
        highest_upload_rate = upload_rate;
    }
    if (download_rate > highest_download_rate) {
        highest_download_rate = download_rate;
    }

    // Insert data into database
    db->insertCPUData(cpu_usage, cpu_freq, used_threads, total_threads, highest_cpu, average_cpu);
    db->insertMemoryData(mem_usage, remaining_ram, average_mem_usage, top_mem_process);
    db->insertNetworkData(upload_rate, download_rate, total_bandwidth_used);

    Logger::log("CLI terminated by user.");
    std::cout << "\nMonitoring stopped by user." << std::endl;
    db->exportToCSV();
    Logger::log("Data exported to CSV files.");
    // Clean up
    delete cpu;
    delete memory;
    delete network;
    delete db;

    return 0;
}
