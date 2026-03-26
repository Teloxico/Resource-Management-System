/**
 * @file alert_manager.h
 * @brief Rule-based alert engine for resource monitoring.
 *
 * AlertManager maintains a set of AlertRules. On each tick the caller
 * invokes evaluate() with the latest MetricData. If a metric breaches
 * its threshold for the configured sustained duration, an AlertEvent is
 * recorded and an optional callback is fired.
 *
 * All public methods are thread-safe (guarded by a mutex).
 * The class does NOT spawn background threads -- the caller drives it.
 */

#pragma once

#include "../metrics.h"

#include <vector>
#include <mutex>
#include <functional>
#include <string>

class AlertManager {
public:
    AlertManager();

    // ---- Rule CRUD ----------------------------------------------------------

    /**
     * @brief Add a new alert rule.
     *
     * The rule's id field is overwritten with a unique auto-incremented value.
     */
    void addRule(const AlertRule& rule);

    /**
     * @brief Remove a rule by its id.
     */
    void removeRule(int id);

    /**
     * @brief Replace an existing rule (matched by id) with updated values.
     *
     * Runtime fields (triggered, currentValue, sustainedCount) are
     * preserved from the existing rule if the caller did not change them.
     */
    void updateRule(const AlertRule& rule);

    /**
     * @brief Return a copy of all rules (including runtime state).
     */
    std::vector<AlertRule> getRules() const;

    // ---- Evaluation ---------------------------------------------------------

    /**
     * @brief Check all enabled rules against the latest metrics.
     *
     * Should be called once per collection tick (typically once per second).
     * Internally increments sustained counters and fires events as needed.
     */
    void evaluate(const MetricData& data);

    // ---- Event log ----------------------------------------------------------

    /**
     * @brief Return a copy of the alert event history (most recent last).
     */
    std::vector<AlertEvent> getEvents() const;

    /**
     * @brief Clear the event history.
     */
    void clearEvents();

    // ---- Callback -----------------------------------------------------------

    using AlertCallback = std::function<void(const AlertEvent&)>;

    /**
     * @brief Register a callback invoked each time an alert fires.
     *
     * Pass nullptr / empty function to unregister.
     */
    void setCallback(AlertCallback cb);

private:
    mutable std::mutex mtx_;

    std::vector<AlertRule>  rules_;
    std::vector<AlertEvent> events_;   ///< Capped at kMaxEvents entries.
    AlertCallback           callback_;
    int                     nextId_ = 1;

    static constexpr size_t kMaxEvents = 1000;

    /**
     * @brief Pull the relevant metric value out of a MetricData bundle.
     */
    float extractMetric(const MetricData& data, AlertMetric metric) const;

    /**
     * @brief Return the current wall-clock time as an ISO 8601 string.
     */
    std::string currentTimestamp() const;
};
