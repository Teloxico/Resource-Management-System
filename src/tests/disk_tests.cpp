/**
 * @file disk_tests.cpp
 * @brief Tests for the Disk monitoring module.
 */

#include <gtest/gtest.h>
#include "core/disk/disk_common.h"

class DiskTest : public ::testing::Test {
protected:
    std::unique_ptr<Disk> disk;
    void SetUp() override {
        disk = createDisk();
        ASSERT_NE(disk, nullptr);
        disk->update();
    }
};

TEST_F(DiskTest, AtLeastOneDisk) {
    auto s = disk->snapshot();
    EXPECT_GE(static_cast<int>(s.disks.size()), 1);
}

TEST_F(DiskTest, DiskSpacePositive) {
    auto s = disk->snapshot();
    for (auto& d : s.disks) {
        EXPECT_GT(d.totalBytes, 0ULL);
        EXPECT_LE(d.usedBytes, d.totalBytes);
        EXPECT_GE(d.usagePercent, 0.0f);
        EXPECT_LE(d.usagePercent, 100.0f);
    }
}
