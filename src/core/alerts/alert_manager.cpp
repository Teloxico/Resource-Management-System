/**
 * @file alert_manager.cpp
 * @brief Implementation of the rule-based alert engine.
 */

#include "alert_manager.h"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AlertManager::AlertManager() = default;

// ---------------------------------------------------------------------------
// Rule CRUD
// ---------------------------------------------------------------------------

void AlertManager::addRule(const AlertRule& rule) {
    std::lock_guard<std::mutex> lock(mtx_);
    AlertRule r = rule;
    r.id             = nextId_++;
    r.triggered      = false;
    r.currentValue   = 0.0f;
    r.sustainedCount = 0;
    r.lastTriggered.clear();
    rules_.push_back(std::move(r));
}

void AlertManager::removeRule(int id) {
    std::lock_guard<std::mutex> lock(mtx_);
    rules_.erase(
        std::remove_if(rules_.begin(), rules_.end(),
                        [id](const AlertRule& r) { return r.id == id; }),
        rules_.end());
}

void AlertManager::updateRule(const AlertRule& rule) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& r : rules_) {
        if (r.id == rule.id) {
            // Preserve runtime state from the existing rule.
            bool   prevTriggered      = r.triggered;
            float  prevCurrentValue   = r.currentValue;
            int    prevSustainedCount = r.sustainedCount;
            std::string prevLast      = r.lastTriggered;

            r = rule;

            // Restore runtime fields unless the caller explicitly changed them.
            // (We cannot tell if caller set them intentionally; the safest
            //  approach is to always preserve runtime state.)
            r.triggered      = prevTriggered;
            r.currentValue   = prevCurrentValue;
            r.sustainedCount = prevSustainedCount;
            r.lastTriggered  = prevLast;
            return;
        }
    }
}

std::vector<AlertRule> AlertManager::getRules() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return rules_;
}

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------

void AlertManager::evaluate(const MetricData& data) {
    std::lock_guard<std::mutex> lock(mtx_);

    for (auto& rule : rules_) {
        if (!rule.enabled) continue;

        float value = extractMetric(data, rule.metric);
        rule.currentValue = value;

        bool conditionMet = rule.above ? (value > rule.threshold)
                                       : (value < rule.threshold);

        if (conditionMet) {
            ++rule.sustainedCount;

            if (rule.sustainedCount >= rule.sustainSeconds && !rule.triggered) {
                rule.triggered = true;

                std::string ts = currentTimestamp();
                rule.lastTriggered = ts;

                // Build human-readable message.
                std::ostringstream msg;
                msg << rule.name << ": value "
                    << std::fixed << std::setprecision(1) << value
                    << (rule.above ? " exceeded " : " dropped below ")
                    << std::fixed << std::setprecision(1) << rule.threshold
                    << " for " << rule.sustainSeconds << "s";

                AlertEvent ev;
                ev.timestamp = ts;
                ev.ruleName  = rule.name;
                ev.message   = msg.str();
                ev.value     = value;
                ev.threshold = rule.threshold;

                events_.push_back(std::move(ev));

                // Cap event history.
                if (events_.size() > kMaxEvents) {
                    events_.erase(events_.begin());
                }

                // Fire callback (still under lock -- keep callback short!).
                if (callback_) {
                    callback_(events_.back());
                }
            }
        } else {
            // Condition no longer met -- reset.
            rule.sustainedCount = 0;
            rule.triggered      = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Event log
// ---------------------------------------------------------------------------

std::vector<AlertEvent> AlertManager::getEvents() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return events_;
}

void AlertManager::clearEvents() {
    std::lock_guard<std::mutex> lock(mtx_);
    events_.clear();
}

// ---------------------------------------------------------------------------
// Callback
// ---------------------------------------------------------------------------

void AlertManager::setCallback(AlertCallback cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    callback_ = std::move(cb);
}

// ---------------------------------------------------------------------------
// extractMetric()
// ---------------------------------------------------------------------------

float AlertManager::extractMetric(const MetricData& data, AlertMetric metric) const {
    switch (metric) {
        case AlertMetric::CpuUsage:
            return data.cpu.totalUsage;

        case AlertMetric::MemoryUsage:
            return data.memory.usagePercent;

        case AlertMetric::SwapUsage:
            return data.memory.swapPercent;

        case AlertMetric::DiskUsage:
            // Aggregate: return highest disk usage among all disks.
            {
                float maxUsage = 0.0f;
                for (const auto& d : data.disk.disks) {
                    if (d.usagePercent > maxUsage)
                        maxUsage = d.usagePercent;
                }
                return maxUsage;
            }

        case AlertMetric::GpuUsage:
            if (!data.gpu.gpus.empty())
                return data.gpu.gpus[0].utilization;
            return 0.0f;

        case AlertMetric::CpuTemp:
            return data.cpu.temperature;

        case AlertMetric::GpuTemp:
            if (!data.gpu.gpus.empty())
                return data.gpu.gpus[0].temperature;
            return -1.0f;

        case AlertMetric::NetUpload:
            return data.network.totalUploadRate;

        case AlertMetric::NetDownload:
            return data.network.totalDownloadRate;

        default:
            return 0.0f;
    }
}

// ---------------------------------------------------------------------------
// currentTimestamp()
// ---------------------------------------------------------------------------

std::string AlertManager::currentTimestamp() const {
    auto now    = std::chrono::system_clock::now();
    auto timeT  = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};

#ifdef _WIN32
    localtime_s(&tmBuf, &timeT);
#else
    localtime_r(&timeT, &tmBuf);
#endif

    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return buf;
}
