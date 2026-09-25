#pragma once

#include <string>

#include "fit/activity.hpp"

// UI-independent formatting helpers. Kept out of the Qt app so they can be
// unit tested without a GUI.
namespace fit {

[[nodiscard]] std::string formatDuration(double seconds);   // "52:13", "1:02:03"
[[nodiscard]] std::string formatPace(double metersPerSecond);  // "5:30 /km"
[[nodiscard]] std::string formatUtc(Timestamp time);  // "2026-09-25 07:30:00 UTC"

}  // namespace fit
