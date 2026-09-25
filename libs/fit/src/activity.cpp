#include "fit/activity.hpp"

namespace fit {
namespace {

// Field numbers from the FIT profile (Profile.xlsx in the Garmin FIT SDK).
namespace file_id {
constexpr std::uint8_t kType = 0, kManufacturer = 1, kProduct = 2, kSerial = 3,
                       kTimeCreated = 4;
}
namespace record {
constexpr std::uint8_t kLat = 0, kLong = 1, kAltitude = 2, kHeartRate = 3, kCadence = 4,
                       kDistance = 5, kSpeed = 6, kFractionalCadence = 53, kEnhancedSpeed = 73,
                       kEnhancedAltitude = 78;
}
namespace session {
constexpr std::uint8_t kStartTime = 2, kSport = 5, kTotalElapsed = 7, kTotalTimer = 8,
                       kTotalDistance = 9, kTotalCycles = 10, kTotalCalories = 11, kAvgSpeed = 14,
                       kMaxSpeed = 15, kAvgHeartRate = 16, kMaxHeartRate = 17, kAvgCadence = 18,
                       kMaxCadence = 19, kTotalAscent = 22, kTotalDescent = 23, kMaxAltitude = 50,
                       kTotalMoving = 59, kMinAltitude = 71, kAvgFractionalCadence = 92,
                       kMaxFractionalCadence = 93, kTotalFractionalCycles = 94,
                       kEnhancedAvgSpeed = 124, kEnhancedMaxSpeed = 125, kEnhancedMinAltitude = 127,
                       kEnhancedMaxAltitude = 128, kAvgStepLength = 134;
}
namespace lap {
constexpr std::uint8_t kStartTime = 2, kTotalTimer = 8, kTotalDistance = 9, kAvgSpeed = 13,
                       kAvgHeartRate = 15, kMaxHeartRate = 16, kAvgCadence = 17, kTotalAscent = 21,
                       kTotalDescent = 22, kAvgFractionalCadence = 80, kEnhancedAvgSpeed = 110;
}
namespace zones_target {
constexpr std::uint8_t kMaxHeartRate = 1;
}
namespace hr_zone {
constexpr std::uint8_t kHighBpm = 1;
}
constexpr std::uint16_t kZonesTargetMesg = 7;
constexpr std::uint16_t kHrZoneMesg = 8;
namespace file_creator {
constexpr std::uint8_t kSoftwareVersion = 0;
}
constexpr std::uint16_t kFileCreatorMesg = 49;

// FIT stores physical values as integers: physical = raw / scale - offset.
std::optional<double> scaled(const Message& m, std::uint8_t field, double scale,
                             double offset = 0.0) {
    if (const auto v = m.intValue(field)) return static_cast<double>(*v) / scale - offset;
    return std::nullopt;
}

std::optional<int> integer(const Message& m, std::uint8_t field) {
    if (const auto v = m.intValue(field)) return static_cast<int>(*v);
    return std::nullopt;
}

std::optional<Timestamp> timeValue(const Message& m, std::uint8_t field) {
    if (const auto v = m.intValue(field)) return toTimestamp(static_cast<std::uint32_t>(*v));
    return std::nullopt;
}

std::optional<double> degrees(const Message& m, std::uint8_t field) {
    if (const auto v = m.intValue(field)) {
        return semicirclesToDegrees(static_cast<std::int32_t>(*v));
    }
    return std::nullopt;
}

// Integer part + optional fractional part (1/128 units), e.g. cadence 74 + 64/128 = 74.5 rpm.
std::optional<double> withFraction(const Message& m, std::uint8_t whole, std::uint8_t fraction) {
    const auto w = m.intValue(whole);
    if (!w) return std::nullopt;
    return static_cast<double>(*w) + static_cast<double>(m.intValue(fraction).value_or(0)) / 128.0;
}

template <typename T>
std::optional<T> firstOf(std::optional<T> preferred, std::optional<T> fallback) {
    return preferred ? preferred : fallback;
}

FileInfo toFileInfo(const Message& m) {
    return {
        .type = integer(m, file_id::kType),
        .manufacturer = integer(m, file_id::kManufacturer),
        .product = integer(m, file_id::kProduct),
        .serialNumber = m.intValue(file_id::kSerial),
        .timeCreated = timeValue(m, file_id::kTimeCreated),
        .softwareVersion = std::nullopt,  // lives in the file_creator message
    };
}

TrackPoint toTrackPoint(const Message& m, Timestamp time) {
    using namespace record;
    return {
        .time = time,
        .latitudeDeg = degrees(m, kLat),
        .longitudeDeg = degrees(m, kLong),
        .altitudeM = firstOf(scaled(m, kEnhancedAltitude, 5.0, 500.0),
                             scaled(m, kAltitude, 5.0, 500.0)),
        .distanceM = scaled(m, kDistance, 100.0),
        .speedMps = firstOf(scaled(m, kEnhancedSpeed, 1000.0), scaled(m, kSpeed, 1000.0)),
        .heartRateBpm = integer(m, kHeartRate),
        .cadence = withFraction(m, kCadence, kFractionalCadence),
    };
}

SessionSummary toSession(const Message& m) {
    using namespace session;
    return {
        .startTime = timeValue(m, kStartTime),
        .sport = integer(m, kSport),
        .totalElapsedS = scaled(m, kTotalElapsed, 1000.0),
        .totalTimerS = scaled(m, kTotalTimer, 1000.0),
        .totalMovingS = scaled(m, kTotalMoving, 1000.0),
        .totalDistanceM = scaled(m, kTotalDistance, 100.0),
        .avgSpeedMps =
            firstOf(scaled(m, kEnhancedAvgSpeed, 1000.0), scaled(m, kAvgSpeed, 1000.0)),
        .maxSpeedMps =
            firstOf(scaled(m, kEnhancedMaxSpeed, 1000.0), scaled(m, kMaxSpeed, 1000.0)),
        .totalAscentM = scaled(m, kTotalAscent, 1.0),
        .totalDescentM = scaled(m, kTotalDescent, 1.0),
        .minAltitudeM =
            firstOf(scaled(m, kEnhancedMinAltitude, 5.0, 500.0), scaled(m, kMinAltitude, 5.0, 500.0)),
        .maxAltitudeM =
            firstOf(scaled(m, kEnhancedMaxAltitude, 5.0, 500.0), scaled(m, kMaxAltitude, 5.0, 500.0)),
        .avgCadence = withFraction(m, kAvgCadence, kAvgFractionalCadence),
        .maxCadence = withFraction(m, kMaxCadence, kMaxFractionalCadence),
        .totalCycles = withFraction(m, kTotalCycles, kTotalFractionalCycles),
        .avgStepLengthM = scaled(m, kAvgStepLength, 10000.0),  // 0.1 mm units
        .totalCalories = integer(m, kTotalCalories),
        .avgHeartRate = integer(m, kAvgHeartRate),
        .maxHeartRate = integer(m, kMaxHeartRate),
    };
}

Lap toLap(const Message& m) {
    using namespace lap;
    return {
        .startTime = timeValue(m, kStartTime),
        .totalTimerS = scaled(m, kTotalTimer, 1000.0),
        .totalDistanceM = scaled(m, kTotalDistance, 100.0),
        .avgSpeedMps =
            firstOf(scaled(m, kEnhancedAvgSpeed, 1000.0), scaled(m, kAvgSpeed, 1000.0)),
        .totalAscentM = scaled(m, kTotalAscent, 1.0),
        .totalDescentM = scaled(m, kTotalDescent, 1.0),
        .avgCadence = withFraction(m, kAvgCadence, kAvgFractionalCadence),
        .avgHeartRate = integer(m, kAvgHeartRate),
        .maxHeartRate = integer(m, kMaxHeartRate),
    };
}

}  // namespace

Timestamp toTimestamp(std::uint32_t fitSeconds) noexcept {
    return Timestamp{std::chrono::seconds{kFitEpochUnixSeconds + std::int64_t{fitSeconds}}};
}

double semicirclesToDegrees(std::int32_t semicircles) noexcept {
    return static_cast<double>(semicircles) * (180.0 / 2147483648.0);  // 2^31 semicircles = 180°
}

std::string sportName(int sport) {
    switch (sport) {
        case 0: return "Generic";
        case 1: return "Running";
        case 2: return "Cycling";
        case 4: return "Fitness equipment";
        case 5: return "Swimming";
        case 10: return "Training";
        case 11: return "Walking";
        case 17: return "Hiking";
        default: return "Sport " + std::to_string(sport);
    }
}

std::string productName(std::optional<int> manufacturer, std::optional<int> product) {
    constexpr int kGarmin = 1;
    if (manufacturer != kGarmin || !product) return product ? "Device " + std::to_string(*product) : "Device";
    switch (*product) {
        // Forerunner 35 and its regional variants (garmin_product in the FIT profile)
        case 2503: case 2650: case 2667: case 2668: case 2727: case 2814:
            return "Forerunner 35";
        case 2431: return "Forerunner 235";
        case 3076: return "Forerunner 245";
        case 3282: return "Forerunner 45";
        case 3869: return "Forerunner 55";
        case 2691: return "Forerunner 935";
        default: return "Garmin " + std::to_string(*product);
    }
}

std::string activityTitle(std::optional<int> sport, int localHour) {
    const char* partOfDay = localHour >= 5 && localHour < 12   ? "Morning"
                            : localHour >= 12 && localHour < 17 ? "Afternoon"
                            : localHour >= 17 && localHour < 22 ? "Evening"
                                                                : "Night";
    std::string noun = "Activity";
    if (sport) {
        switch (*sport) {
            case 1: noun = "Run"; break;
            case 2: noun = "Ride"; break;
            case 5: noun = "Swim"; break;
            case 11: noun = "Walk"; break;
            case 17: noun = "Hike"; break;
            case 4: case 10: noun = "Workout"; break;
            default: break;
        }
    }
    return std::string(partOfDay) + ' ' + noun;
}

Activity toActivity(const DecodedFile& decoded) {
    Activity activity;
    for (const Message& m : decoded.messages) {
        switch (m.globalNumber) {
            case mesg::kFileId: {
                const auto version = activity.file.softwareVersion;  // file_creator may come first
                activity.file = toFileInfo(m);
                activity.file.softwareVersion = version;
                break;
            }
            case mesg::kSession:
                activity.sessions.push_back(toSession(m));
                break;
            case mesg::kLap:
                activity.laps.push_back(toLap(m));
                break;
            case kZonesTargetMesg:
                if (const auto max = integer(m, zones_target::kMaxHeartRate)) activity.heartRate.maxHeartRate = max;
                break;
            case kHrZoneMesg:
                if (const auto high = integer(m, hr_zone::kHighBpm)) activity.heartRate.zoneHighBpm.push_back(*high);
                break;
            case kFileCreatorMesg:
                activity.file.softwareVersion = scaled(m, file_creator::kSoftwareVersion, 100.0);
                break;
            case mesg::kRecord:
                // A track point without a time is useless for plotting.
                if (const auto time = timeValue(m, kTimestampField)) {
                    activity.points.push_back(toTrackPoint(m, *time));
                }
                break;
            default:
                break;
        }
    }
    return activity;
}

Activity loadActivity(const std::filesystem::path& path) { return toActivity(decodeFile(path)); }

}  // namespace fit
