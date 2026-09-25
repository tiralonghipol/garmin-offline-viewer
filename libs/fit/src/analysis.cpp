#include "fit/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fit {

Climb climb(const std::vector<double>& altitudes, double thresholdM) {
    Climb result;
    if (altitudes.empty()) return result;
    double reference = altitudes.front();
    for (const double alt : altitudes) {
        if (alt >= reference + thresholdM) {
            result.ascentM += alt - reference;
            reference = alt;
        } else if (alt <= reference - thresholdM) {
            result.descentM += reference - alt;
            reference = alt;
        }
    }
    return result;
}

std::vector<double> movingAverage(const std::vector<double>& values, std::size_t radius) {
    std::vector<double> out(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        const std::size_t first = i >= radius ? i - radius : 0;
        const std::size_t last = std::min(values.size() - 1, i + radius);
        double sum = 0.0;
        for (std::size_t j = first; j <= last; ++j) sum += values[j];
        out[i] = sum / static_cast<double>(last - first + 1);
    }
    return out;
}

std::optional<double> percentile(std::vector<double> values, double p) {
    if (values.empty()) return std::nullopt;
    p = std::clamp(p, 0.0, 1.0);
    const auto rank = static_cast<std::size_t>(std::lround(p * static_cast<double>(values.size() - 1)));
    const auto nth = values.begin() + static_cast<std::ptrdiff_t>(rank);
    std::nth_element(values.begin(), nth, values.end());
    return *nth;
}

Totals summarize(const Activity& activity) {
    const SessionSummary session = activity.sessions.empty() ? SessionSummary{} : activity.sessions.front();
    const auto& points = activity.points;

    Totals t;
    t.sport = session.sport;
    t.start = session.startTime;
    if (!t.start && !points.empty()) t.start = points.front().time;
    if (!t.start) t.start = activity.file.timeCreated;

    t.distanceM = session.totalDistanceM;
    if (!t.distanceM) {
        for (auto it = points.rbegin(); it != points.rend(); ++it) {
            if (it->distanceM) {
                t.distanceM = it->distanceM;
                break;
            }
        }
    }

    t.timerS = session.totalTimerS ? session.totalTimerS : session.totalElapsedS;
    if (!t.timerS && points.size() > 1) {
        t.timerS = static_cast<double>((points.back().time - points.front().time).count());
    }

    t.avgSpeedMps = session.avgSpeedMps;
    if (!t.avgSpeedMps && t.distanceM && t.timerS && *t.timerS > 0) {
        t.avgSpeedMps = *t.distanceM / *t.timerS;
    }
    t.maxSpeedMps = session.maxSpeedMps;

    t.ascentM = session.totalAscentM;
    t.descentM = session.totalDescentM;
    if (!t.ascentM) {
        std::vector<double> altitudes;
        for (const auto& p : points) {
            if (p.altitudeM) altitudes.push_back(*p.altitudeM);
        }
        if (!altitudes.empty()) {
            const Climb c = climb(altitudes);
            t.ascentM = c.ascentM;
            t.descentM = c.descentM;
        }
    }

    t.avgHeartRate = session.avgHeartRate;
    t.maxHeartRate = session.maxHeartRate;
    if (!t.avgHeartRate || !t.maxHeartRate) {
        long long sum = 0;
        int count = 0;
        int max = 0;
        for (const auto& p : points) {
            if (p.heartRateBpm) {
                sum += *p.heartRateBpm;
                ++count;
                max = std::max(max, *p.heartRateBpm);
            }
        }
        if (count > 0) {
            if (!t.avgHeartRate) t.avgHeartRate = static_cast<int>(std::lround(static_cast<double>(sum) / count));
            if (!t.maxHeartRate) t.maxHeartRate = max;
        }
    }
    t.calories = session.totalCalories;
    return t;
}

}  // namespace fit
