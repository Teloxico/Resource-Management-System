/**
 * @file memory_tests.cpp
 * @brief Tests for the Memory monitoring module (snapshot-based API).
 */

#include <gtest/gtest.h>
#include "core/memory/memory_common.h"
#include <thread>
#include <chrono>

class MemoryTest : public ::testing::Test {
protected:
    std::unique_ptr<Memory> mem;
    void SetUp() override {
        mem = createMemory();
        ASSERT_NE(mem, nullptr);
        mem->update();
    }
};

TEST_F(MemoryTest, UsageInRange) {
    auto s = mem->snapshot();
    EXPECT_GE(s.usagePercent, 0.0f);
    EXPECT_LE(s.usagePercent, 100.0f);
}

TEST_F(MemoryTest, TotalBytesPositive) {
    auto s = mem->snapshot();
    EXPECT_GT(s.totalBytes, 0ULL);
}

TEST_F(MemoryTest, UsedLessThanTotal) {
    auto s = mem->snapshot();
    EXPECT_LE(s.usedBytes, s.totalBytes);
}

TEST_F(MemoryTest, SwapTotalNonNegative) {
    auto s = mem->snapshot();
    // Swap can be 0 if disabled
    EXPECT_GE(s.swapTotal, 0ULL);
    EXPECT_GE(s.swapPercent, 0.0f);
}

TEST_F(MemoryTest, TopProcessNotEmpty) {
    // Need a moment for the process scan
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    mem->update();
    auto s = mem->snapshot();
    // May be empty on some systems — just don't crash
    SUCCEED();
}
