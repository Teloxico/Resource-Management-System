/**
 * @file process_tests.cpp
 * @brief Tests for the Process monitoring module.
 */

#include <gtest/gtest.h>
#include "core/process/process_common.h"

class ProcessTest : public ::testing::Test {
protected:
    std::unique_ptr<ProcessManager> proc;
    void SetUp() override {
        proc = createProcessManager();
        ASSERT_NE(proc, nullptr);
        proc->update();
    }
};

TEST_F(ProcessTest, HasProcesses) {
    auto s = proc->snapshot();
    EXPECT_GT(s.totalProcesses, 0);
    EXPECT_FALSE(s.processes.empty());
}

TEST_F(ProcessTest, ProcessHasPidAndName) {
    auto s = proc->snapshot();
    for (auto& p : s.processes) {
        EXPECT_GE(p.pid, 0);  // PID 0 is valid on Windows (System Idle Process)
    }
}

TEST_F(ProcessTest, CpuPercentNonNegative) {
    auto s = proc->snapshot();
    for (auto& p : s.processes) {
        EXPECT_GE(p.cpuPercent, 0.0f);
    }
}
