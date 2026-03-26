/**
 * @file cpu_tests.cpp
 * @brief Tests for the CPU monitoring module (snapshot-based API).
 */

#include <gtest/gtest.h>
#include "core/cpu/cpu_common.h"
#include <thread>
#include <chrono>

class CpuTest : public ::testing::Test {
protected:
    std::unique_ptr<CPU> cpu;
    void SetUp() override {
        cpu = createCPU();
        ASSERT_NE(cpu, nullptr);
        cpu->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        cpu->update();
    }
};

TEST_F(CpuTest, TotalUsageInRange) {
    auto s = cpu->snapshot();
    EXPECT_GE(s.totalUsage, 0.0f);
    EXPECT_LE(s.totalUsage, 100.0f);
}

TEST_F(CpuTest, FrequencyPositive) {
    auto s = cpu->snapshot();
    EXPECT_GT(s.frequency, 0.0f);
}

TEST_F(CpuTest, LogicalCoresPositive) {
    auto s = cpu->snapshot();
    EXPECT_GT(s.logicalCores, 0);
}

TEST_F(CpuTest, PerCoreCount) {
    auto s = cpu->snapshot();
    EXPECT_EQ(static_cast<int>(s.cores.size()), s.logicalCores);
}

TEST_F(CpuTest, PerCoreUsageInRange) {
    auto s = cpu->snapshot();
    for (auto& c : s.cores) {
        EXPECT_GE(c.usage, 0.0f);
        EXPECT_LE(c.usage, 100.0f);
    }
}

TEST_F(CpuTest, AverageAndHighest) {
    auto s = cpu->snapshot();
    EXPECT_GE(s.averageUsage, 0.0f);
    EXPECT_LE(s.averageUsage, 100.0f);
    EXPECT_GE(s.highestUsage, 0.0f);
}
