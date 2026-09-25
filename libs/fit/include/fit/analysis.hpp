#pragma once

#include <array>
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
    std::optional<double> timerS;    // timer running (excludes auto-pause / stop)
    std::optional<double> elapsedS;  // wall clock, start to finish
    std::optional<double> movingS;   // timer time actually moving
    std::optional<double> avgSpeedMps;
    std::optional<double> avgMovingSpeedMps;
    std::optional<double> maxSpeedMps;  // Garmin's "Best Pace"
    std::optional<double> ascentM;
    std::optional<double> descentM;
    std::optional<double> minAltitudeM;
    std::optional<double> maxAltitudeM;
    std::optional<double> avgCadence;  // rpm; running: x2 for steps/min
    std::optional<double> maxCadence;
    std::optional<double> avgStepLengthM;
    std::optional<int> avgHeartRate;
    std::optional<int> maxHeartRate;
    std::optional<int> calories;
};

[[nodiscard]] Totals summarize(const Activity& activity);

// Samples slower than this count as standing still for moving time.
inline constexpr double kMovingThresholdMps = 0.5;
// Gaps between records longer than this are pauses (auto-pause, stop button).
inline constexpr double kPauseGapS = 60.0;

// Time spent moving, from the track points (fallback when the watch doesn't record it).
[[nodiscard]] std::optional<double> movingTime(const std::vector<TrackPoint>& points);

// --- heart-rate zones ---------------------------------------------------------

struct HeartRateZones {
    std::array<int, 5> lowerBpm{};  // zone 1..5 start at these bpm (inclusive)
    int maxHeartRate = 0;
    bool fromDevice = false;        // true: boundaries the watch stored in the file
};

// Garmin's defaults: zones start at 50/60/70/80/90 % of max HR.
[[nodiscard]] HeartRateZones zonesFromMaxHeartRate(int maxHeartRate);
// Boundaries stored in the file, if usable. Accepts both layouts seen in the
// wild: 6 ceilings (zone 0..5, Garmin) or 5 ceilings (zone 1..5, Wahoo).
[[nodiscard]] std::optional<HeartRateZones> zonesFromDevice(const HeartRateSettings& settings);

// 0 = below zone 1, 1..5 = zone.
[[nodiscard]] int zoneOf(int bpm, const HeartRateZones& zones);
// Garmin-style decimal zone, e.g. 2.8 = 80 % through zone 2. Below zone 1 is < 1.
[[nodiscard]] double fractionalZone(double bpm, const HeartRateZones& zones);
// Seconds in [below zone 1, zone 1, ..., zone 5]; pauses are not counted.
[[nodiscard]] std::array<double, 6> timeInZones(const std::vector<TrackPoint>& points,
                                                const HeartRateZones& zones);

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
