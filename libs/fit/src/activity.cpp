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
                       kDistance = 5, kSpeed = 6, kEnhancedSpeed = 73, kEnhancedAltitude = 78;
}
namespace session {
constexpr std::uint8_t kStartTime = 2, kSport = 5, kTotalElapsed = 7, kTotalTimer = 8,
                       kTotalDistance = 9, kTotalCalories = 11, kAvgSpeed = 14, kAvgHeartRate = 16,
                       kMaxHeartRate = 17, kEnhancedAvgSpeed = 124;
}

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
        .cadence = integer(m, kCadence),
    };
}

SessionSummary toSession(const Message& m) {
    using namespace session;
    return {
        .startTime = timeValue(m, kStartTime),
        .sport = integer(m, kSport),
        .totalElapsedS = scaled(m, kTotalElapsed, 1000.0),
        .totalTimerS = scaled(m, kTotalTimer, 1000.0),
        .totalDistanceM = scaled(m, kTotalDistance, 100.0),
        .avgSpeedMps =
            firstOf(scaled(m, kEnhancedAvgSpeed, 1000.0), scaled(m, kAvgSpeed, 1000.0)),
        .totalCalories = integer(m, kTotalCalories),
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

Activity toActivity(const DecodedFile& decoded) {
    Activity activity;
    for (const Message& m : decoded.messages) {
        switch (m.globalNumber) {
            case mesg::kFileId:
                activity.file = toFileInfo(m);
                break;
            case mesg::kSession:
                activity.sessions.push_back(toSession(m));
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
