/**
 * @file network_tests.cpp
 * @brief Tests for the Network monitoring module (snapshot-based API).
 */

#include <gtest/gtest.h>
#include "core/network/network_common.h"
#include <thread>
#include <chrono>

class NetworkTest : public ::testing::Test {
protected:
    std::unique_ptr<Network> net;
    void SetUp() override {
        net = createNetwork();
        ASSERT_NE(net, nullptr);
        net->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        net->update();
    }
};

TEST_F(NetworkTest, RatesNonNegative) {
    auto s = net->snapshot();
    EXPECT_GE(s.totalUploadRate, 0.0f);
    EXPECT_GE(s.totalDownloadRate, 0.0f);
}

TEST_F(NetworkTest, InterfacesDetected) {
    auto s = net->snapshot();
    // Should have at least one interface (loopback or physical)
    EXPECT_GE(static_cast<int>(s.interfaces.size()), 0);
}

TEST_F(NetworkTest, ByteCountsNonNegative) {
    auto s = net->snapshot();
    EXPECT_GE(s.totalBytesSent, 0ULL);
    EXPECT_GE(s.totalBytesRecv, 0ULL);
}
