#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "fit/activity.hpp"

// Derived numbers for display. Prefers what the watch computed (session
// message) and falls back to computing from the track points.
namespace fit {

struct Totals {
    std::optional<Timestamp> start;
    std::optional<int> sport;
    std::optional<double> distanceM;
    std::optional<double> timerS;  // moving time (excludes pauses)
    std::optional<double> avgSpeedMps;
    std::optional<double> maxSpeedMps;
    std::optional<double> ascentM;
    std::optional<double> descentM;
    std::optional<int> avgHeartRate;
    std::optional<int> maxHeartRate;
    std::optional<int> calories;
};

[[nodiscard]] Totals summarize(const Activity& activity);

struct Climb {
    double ascentM = 0.0;
    double descentM = 0.0;
};
// Sums altitude changes larger than `thresholdM` (hysteresis), so GPS noise
// on a flat route does not add up to phantom climbing.
[[nodiscard]] Climb climb(const std::vector<double>& altitudes, double thresholdM = 3.0);

// Centered moving average over 2*radius+1 samples (shrinks at the edges).
[[nodiscard]] std::vector<double> movingAverage(const std::vector<double>& values,
                                                std::size_t radius);

// Nearest-rank percentile, p in [0, 1]. nullopt for an empty input.
[[nodiscard]] std::optional<double> percentile(std::vector<double> values, double p);

}  // namespace fit
