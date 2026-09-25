#include "fit/format.hpp"

#include <cmath>
#include <format>

namespace fit {

std::string formatDuration(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) return "--";
    const long long total = std::llround(seconds);
    const long long h = total / 3600;
    const long long m = (total / 60) % 60;
    const long long s = total % 60;
    return h > 0 ? std::format("{}:{:02}:{:02}", h, m, s) : std::format("{}:{:02}", m, s);
}

std::string formatPace(double metersPerSecond) {
    // Below ~0.2 m/s (>80 min/km) you're standing still; a pace is meaningless.
    if (!std::isfinite(metersPerSecond) || metersPerSecond < 0.2) return "--";
    const long long secondsPerKm = std::llround(1000.0 / metersPerSecond);
    return std::format("{}:{:02} /km", secondsPerKm / 60, secondsPerKm % 60);
}

std::string formatUtc(Timestamp time) { return std::format("{:%Y-%m-%d %H:%M:%S} UTC", time); }

}  // namespace fit
