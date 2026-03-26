/**
 * @file alert_tests.cpp
 * @brief Tests for the AlertManager.
 */

#include <gtest/gtest.h>
#include "core/alerts/alert_manager.h"

class AlertTest : public ::testing::Test {
protected:
    AlertManager mgr;
};

TEST_F(AlertTest, AddAndListRules) {
    AlertRule r;
    r.name = "High CPU";
    r.metric = AlertMetric::CpuUsage;
    r.threshold = 90.0f;
    r.above = true;
    r.sustainSeconds = 1;
    mgr.addRule(r);

    auto rules = mgr.getRules();
    ASSERT_EQ(rules.size(), 1u);
    EXPECT_EQ(rules[0].name, "High CPU");
}

TEST_F(AlertTest, TriggersWhenAboveThreshold) {
    AlertRule r;
    r.name = "CPU Alert";
    r.metric = AlertMetric::CpuUsage;
    r.threshold = 50.0f;
    r.above = true;
    r.sustainSeconds = 1;
    mgr.addRule(r);

    MetricData md{};
    md.cpu.totalUsage = 80.0f; // above threshold
    mgr.evaluate(md);

    auto rules = mgr.getRules();
    EXPECT_TRUE(rules[0].triggered);

    auto events = mgr.getEvents();
    EXPECT_GE(events.size(), 1u);
}

TEST_F(AlertTest, DoesNotTriggerBelowThreshold) {
    AlertRule r;
    r.name = "CPU Alert";
    r.metric = AlertMetric::CpuUsage;
    r.threshold = 50.0f;
    r.above = true;
    r.sustainSeconds = 1;
    mgr.addRule(r);

    MetricData md{};
    md.cpu.totalUsage = 30.0f; // below threshold
    mgr.evaluate(md);

    auto rules = mgr.getRules();
    EXPECT_FALSE(rules[0].triggered);
}

TEST_F(AlertTest, RemoveRule) {
    AlertRule r;
    r.name = "test";
    r.metric = AlertMetric::MemoryUsage;
    r.threshold = 80;
    mgr.addRule(r);

    auto rules = mgr.getRules();
    ASSERT_EQ(rules.size(), 1u);
    mgr.removeRule(rules[0].id);

    rules = mgr.getRules();
    EXPECT_TRUE(rules.empty());
}
