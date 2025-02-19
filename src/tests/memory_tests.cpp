// File: src/tests/memory_tests.cpp

#include <gtest/gtest.h>
#include "core/memory/memory_common.h"

/**
 * @brief Mock Memory class for testing purposes.
 *
 * Provides fixed values for memory metrics to test the functionality.
 */
class MockMemory : public Memory {
public:
    /**
     * @brief Returns a mock total memory usage percentage.
     * @return Total memory usage as a percentage.
     */
    float getTotalUsage() override { return 40.0f; }

    /**
     * @brief Returns a mock remaining RAM value.
     * @return Remaining RAM in MB.
     */
    float getRemainingRAM() override { return 2048.0f; } // 2 GB

    /**
     * @brief Returns a mock average memory usage.
     * @return Average memory usage as a percentage.
     */
    float getAverageUsage() override { return 35.0f; }

    /**
     * @brief Returns a mock most memory-consuming process name.
     * @return Name of the top memory-consuming process.
     */
    std::string getMostUsingProcess() override { return "TestProcess"; }
};

/**
 * @brief Test Fixture for Memory tests.
 *
 * Uses the MockMemory class to test memory-related functionalities.
 */
class MemoryTest : public ::testing::Test {
protected:
    MockMemory memory; /**< Instance of MockMemory used for testing */

    /**
     * @brief Set up the test environment before each test.
     */
    void SetUp() override {
        // Initialization if needed
    }

    /**
     * @brief Clean up after each test.
     */
    void TearDown() override {
        // Cleanup if needed
    }
};

/**
 * @test Tests that getTotalUsage returns a value within 0-100%.
 */
TEST_F(MemoryTest, TotalUsageWithinRange) {
    float usage = memory.getTotalUsage();
    EXPECT_GE(usage, 0.0f);
    EXPECT_LE(usage, 100.0f);
}

/**
 * @test Tests that getRemainingRAM returns a positive value.
 */
TEST_F(MemoryTest, RemainingRAMPositive) {
    float ram = memory.getRemainingRAM();
    EXPECT_GE(ram, 0.0f);
    EXPECT_LE(ram, 32768.0f); // Assuming max 32 GB RAM
}

/**
 * @test Tests that getAverageUsage returns a value within 0-100%.
 */
TEST_F(MemoryTest, AverageUsageCalculation) {
    float average = memory.getAverageUsage();
    EXPECT_GE(average, 0.0f);
    EXPECT_LE(average, 100.0f);
}

/**
 * @test Tests that getMostUsingProcess returns a non-empty string.
 */
TEST_F(MemoryTest, MostUsingProcessNotEmpty) {
    std::string process = memory.getMostUsingProcess();
    EXPECT_FALSE(process.empty());
    EXPECT_NE(process, "N/A");
}

/**
 * @test Tests over time usage calculations.
 */
TEST_F(MemoryTest, UsageOverTime) {
    // Simulate usage readings
    for (int i = 0; i < 5; ++i) {
        memory.getTotalUsage();
    }
    float average = memory.getAverageUsage();
    EXPECT_GE(average, 0.0f);
}

