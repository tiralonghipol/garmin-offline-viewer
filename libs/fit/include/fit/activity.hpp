#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fit/decoder.hpp"

namespace fit {

using Timestamp = std::chrono::sys_seconds;

// FIT timestamps count seconds since 1989-12-31T00:00:00Z.
inline constexpr std::int64_t kFitEpochUnixSeconds = 631065600;

[[nodiscard]] Timestamp toTimestamp(std::uint32_t fitSeconds) noexcept;
[[nodiscard]] double semicirclesToDegrees(std::int32_t semicircles) noexcept;
[[nodiscard]] std::string sportName(int sport);
// "Forerunner 35" for known Garmin products, otherwise "Garmin 1234" / "Device".
[[nodiscard]] std::string productName(std::optional<int> manufacturer, std::optional<int> product);
// Strava-style name from sport and local start hour, e.g. "Morning Run".
[[nodiscard]] std::string activityTitle(std::optional<int> sport, int localHour);

struct FileInfo {
    std::optional<int> type;          // 4 = activity
    std::optional<int> manufacturer;  // 1 = Garmin
    std::optional<int> product;
    std::optional<std::int64_t> serialNumber;
    std::optional<Timestamp> timeCreated;
    std::optional<double> softwareVersion;  // from file_creator, e.g. 3.60
};

struct TrackPoint {
    Timestamp time{};
    std::optional<double> latitudeDeg;
    std::optional<double> longitudeDeg;
    std::optional<double> altitudeM;
    std::optional<double> distanceM;
    std::optional<double> speedMps;
    std::optional<int> heartRateBpm;
    std::optional<int> cadence;  // FIT running cadence is strides/min (x2 = steps/min)
};

struct SessionSummary {
    std::optional<Timestamp> startTime;
    std::optional<int> sport;
    std::optional<double> totalElapsedS;
    std::optional<double> totalTimerS;
    std::optional<double> totalDistanceM;
    std::optional<double> avgSpeedMps;
    std::optional<double> maxSpeedMps;
    std::optional<double> totalAscentM;
    std::optional<double> totalDescentM;
    std::optional<int> totalCalories;
    std::optional<int> avgHeartRate;
    std::optional<int> maxHeartRate;
};

// A lap is one split: auto-lap (every 1 km by default on the FR35) or a button press.
struct Lap {
    std::optional<Timestamp> startTime;
    std::optional<double> totalTimerS;
    std::optional<double> totalDistanceM;
    std::optional<double> avgSpeedMps;
    std::optional<double> totalAscentM;
    std::optional<int> avgHeartRate;
    std::optional<int> maxHeartRate;
};

// High-level view of an activity file: the parts a viewer cares about.
struct Activity {
    FileInfo file;
    std::vector<SessionSummary> sessions;
    std::vector<Lap> laps;
    std::vector<TrackPoint> points;
};

[[nodiscard]] Activity toActivity(const DecodedFile& decoded);
[[nodiscard]] Activity loadActivity(const std::filesystem::path& path);

}  // namespace fit
