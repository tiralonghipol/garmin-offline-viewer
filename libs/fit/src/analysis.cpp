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

namespace {

// Speed at point i: recorded, or derived from the distance delta.
std::optional<double> speedAt(const std::vector<TrackPoint>& points, std::size_t i) {
    if (points[i].speedMps) return points[i].speedMps;
    if (i == 0 || !points[i].distanceM || !points[i - 1].distanceM) return std::nullopt;
    const auto dt = static_cast<double>((points[i].time - points[i - 1].time).count());
    if (dt <= 0 || dt > kPauseGapS) return std::nullopt;
    return (*points[i].distanceM - *points[i - 1].distanceM) / dt;
}

}  // namespace

std::optional<double> movingTime(const std::vector<TrackPoint>& points) {
    if (points.size() < 2) return std::nullopt;
    double moving = 0.0;
    bool anySpeed = false;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const auto dt = static_cast<double>((points[i].time - points[i - 1].time).count());
        const auto speed = speedAt(points, i);
        anySpeed = anySpeed || speed.has_value();
        if (dt > 0 && dt <= kPauseGapS && speed && *speed > kMovingThresholdMps) moving += dt;
    }
    return anySpeed ? std::optional{moving} : std::nullopt;
}

HeartRateZones zonesFromMaxHeartRate(int maxHeartRate) {
    HeartRateZones z;
    z.maxHeartRate = maxHeartRate;
    constexpr std::array<double, 5> kPercent{0.5, 0.6, 0.7, 0.8, 0.9};
    for (std::size_t i = 0; i < 5; ++i) {
        z.lowerBpm[i] = static_cast<int>(std::lround(kPercent[i] * maxHeartRate));
    }
    return z;
}

std::optional<HeartRateZones> zonesFromDevice(const HeartRateSettings& settings) {
    const auto& h = settings.zoneHighBpm;
    if (h.size() != 5 && h.size() != 6) return std::nullopt;
    if (!std::is_sorted(h.begin(), h.end())) return std::nullopt;
    HeartRateZones z;
    z.fromDevice = true;
    if (h.size() == 6) {  // [top of zone 0, top of zone 1, ..., top of zone 5]
        for (std::size_t i = 0; i < 5; ++i) z.lowerBpm[i] = h[i] + 1;
    } else {              // [top of zone 1, ..., top of zone 5]; zone 1 starts at 0
        z.lowerBpm[0] = 0;
        for (std::size_t i = 1; i < 5; ++i) z.lowerBpm[i] = h[i - 1] + 1;
    }
    // Max HR: stated explicitly, else the top of zone 5 unless it's an open-ended 254/255 marker.
    constexpr int kOpenEnded = 250;
    z.maxHeartRate = settings.maxHeartRate.value_or(h.back() < kOpenEnded ? h.back() : 0);
    return z;
}

int zoneOf(int bpm, const HeartRateZones& zones) {
    int zone = 0;
    for (std::size_t i = 0; i < 5; ++i) {
        if (bpm >= zones.lowerBpm[i]) zone = static_cast<int>(i) + 1;
    }
    return zone;
}

double fractionalZone(double bpm, const HeartRateZones& zones) {
    const int zone = zoneOf(static_cast<int>(std::floor(bpm)), zones);
    if (zone == 0) return zones.lowerBpm[0] > 0 ? bpm / zones.lowerBpm[0] : 0.0;
    const double lower = zones.lowerBpm[static_cast<std::size_t>(zone - 1)];
    const double upper = zone < 5 ? zones.lowerBpm[static_cast<std::size_t>(zone)]
                                  : std::max<double>(zones.maxHeartRate + 1, lower + 1);
    return zone + std::clamp((bpm - lower) / (upper - lower), 0.0, 0.99);
}

std::array<double, 6> timeInZones(const std::vector<TrackPoint>& points, const HeartRateZones& zones) {
    std::array<double, 6> seconds{};
    for (std::size_t i = 1; i < points.size(); ++i) {
        const auto dt = static_cast<double>((points[i].time - points[i - 1].time).count());
        const auto& hr = points[i - 1].heartRateBpm;  // HR holds until the next sample
        if (!hr || dt <= 0 || dt > kPauseGapS) continue;
        seconds[static_cast<std::size_t>(zoneOf(*hr, zones))] += dt;
    }
    return seconds;
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

    t.elapsedS = session.totalElapsedS;
    if (!t.elapsedS && points.size() > 1) {
        t.elapsedS = static_cast<double>((points.back().time - points.front().time).count());
    }
    t.timerS = session.totalTimerS ? session.totalTimerS : t.elapsedS;
    t.movingS = session.totalMovingS ? session.totalMovingS : movingTime(points);
    if (t.movingS && t.timerS) t.movingS = std::min(*t.movingS, *t.timerS);

    t.avgSpeedMps = session.avgSpeedMps;
    if (!t.avgSpeedMps && t.distanceM && t.timerS && *t.timerS > 0) {
        t.avgSpeedMps = *t.distanceM / *t.timerS;
    }
    if (t.distanceM && t.movingS && *t.movingS > 0) t.avgMovingSpeedMps = *t.distanceM / *t.movingS;
    t.maxSpeedMps = session.maxSpeedMps;
    if (!t.maxSpeedMps) {  // best speed held over ~10 samples, so one GPS jump doesn't count
        std::vector<double> speeds;
        for (std::size_t i = 0; i < points.size(); ++i) speeds.push_back(speedAt(points, i).value_or(0.0));
        if (!speeds.empty()) {
            const auto smooth = movingAverage(speeds, 5);
            const double best = *std::max_element(smooth.begin(), smooth.end());
            if (best > 0) t.maxSpeedMps = best;
        }
    }

    t.ascentM = session.totalAscentM;
    t.descentM = session.totalDescentM;
    t.minAltitudeM = session.minAltitudeM;
    t.maxAltitudeM = session.maxAltitudeM;
    std::vector<double> altitudes;
    for (const auto& p : points) {
        if (p.altitudeM) altitudes.push_back(*p.altitudeM);
    }
    if (!altitudes.empty()) {
        const Climb c = climb(altitudes);
        if (!t.ascentM) t.ascentM = c.ascentM;
        if (!t.descentM) t.descentM = c.descentM;
        const auto [lo, hi] = std::minmax_element(altitudes.begin(), altitudes.end());
        if (!t.minAltitudeM) t.minAltitudeM = *lo;
        if (!t.maxAltitudeM) t.maxAltitudeM = *hi;
    }

    // Cadence: zeros (standing still) are ignored, as Garmin does for the average.
    t.avgCadence = session.avgCadence;
    t.maxCadence = session.maxCadence;
    if (!t.avgCadence || !t.maxCadence) {
        double sum = 0.0, max = 0.0;
        int count = 0;
        for (const auto& p : points) {
            if (p.cadence && *p.cadence > 0) {
                sum += *p.cadence;
                max = std::max(max, *p.cadence);
                ++count;
            }
        }
        if (count > 0) {
            if (!t.avgCadence) t.avgCadence = sum / count;
            if (!t.maxCadence) t.maxCadence = max;
        }
    }
    // Step length: recorded, else distance / steps (a running "cycle" is a stride = 2 steps).
    constexpr int kRunning = 1;
    t.avgStepLengthM = session.avgStepLengthM;
    if (!t.avgStepLengthM && t.distanceM && t.sport == kRunning) {
        std::optional<double> steps;
        if (session.totalCycles && *session.totalCycles > 0) steps = *session.totalCycles * 2.0;
        else if (t.avgCadence && t.movingS) steps = *t.avgCadence * 2.0 * *t.movingS / 60.0;
        if (steps && *steps > 0) t.avgStepLengthM = *t.distanceM / *steps;
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
