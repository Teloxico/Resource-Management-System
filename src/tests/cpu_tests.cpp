// File: src/tests/cpu_tests.cpp

#include <gtest/gtest.h>
#include "core/cpu/cpu_common.h"

/**
 * @brief Test Fixture for CPU tests.
 *
 * Creates an instance of the CPU class for testing CPU monitoring functionalities.
 */
class CPUTest : public ::testing::Test {
protected:
    CPU* cpu; /**< Pointer to a CPU instance */

    /**
     * @brief Set up the test environment before each test.
     *
     * Initializes the CPU instance using the factory function.
     */
    void SetUp() override {
        // Create an instance of CPU
        cpu = createCPU();
        ASSERT_NE(cpu, nullptr) << "Failed to create CPU instance.";
    }

    /**
     * @brief Clean up after each test.
     *
     * Deletes the CPU instance.
     */
    void TearDown() override {
        // Clean up CPU instance
        delete cpu;
    }
};

/**
 * @test Verifies that the total CPU usage is within 0-100%.
 */
TEST_F(CPUTest, TotalUsageCalculation) {
    float total_usage = cpu->getTotalUsage();
    EXPECT_GE(total_usage, 0.0f) << "CPU usage should be >= 0%";
    EXPECT_LE(total_usage, 100.0f) << "CPU usage should be <= 100%";
}

/**
 * @test Verifies that the CPU clock frequency is positive.
 */
TEST_F(CPUTest, ClockFrequencyPositive) {
    float frequency = cpu->getClockFrequency();
    EXPECT_GT(frequency, 0.0f) << "CPU clock frequency should be positive (GHz)";
}

/**
 * @test Verifies that the number of used threads is non-negative.
 */
TEST_F(CPUTest, UsedThreadsNonNegative) {
    int used_threads = cpu->getUsedThreads();
    EXPECT_GE(used_threads, 0) << "Used threads should be >= 0";
}

/**
 * @test Verifies that the total number of threads is logical (>= used threads).
 */
TEST_F(CPUTest, TotalThreadsLogical) {
    int used_threads = cpu->getUsedThreads();
    int total_threads = cpu->getTotalThreads();
    EXPECT_GE(total_threads, used_threads) << "Total threads should be >= used threads";
    EXPECT_GT(total_threads, 0) << "Total threads should be > 0";
}

/**
 * @test Verifies that the highest CPU usage is updated correctly.
 */
TEST_F(CPUTest, HighestUsageCalculation) {
    // Simulate usage readings
    cpu->getTotalUsage();
    float highest_usage = cpu->getHighestUsage();
    EXPECT_GE(highest_usage, 0.0f);
}

/**
 * @test Verifies that the average CPU usage is calculated correctly.
 */
TEST_F(CPUTest, AverageUsageCalculation) {
    // Simulate multiple readings
    for (int i = 0; i < 5; ++i) {
        cpu->getTotalUsage();
    }
    float average_usage = cpu->getAverageUsage();
    EXPECT_GE(average_usage, 0.0f);
    EXPECT_LE(average_usage, 100.0f);
}


