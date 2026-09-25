#pragma once

#include <QString>
#include <optional>
#include <vector>

#include "fit/analysis.hpp"

// Locale-aware text for the numbers shown in the list and detail pages.
// Missing values render as "--", as Garmin Connect does.
namespace metrics {

QString distance(std::optional<double> meters);  // "1,21 km"
QString duration(std::optional<double> seconds);  // "25:09"
QString pace(std::optional<double> metersPerSecond);  // "5:24 /km"
QString speed(std::optional<double> metersPerSecond);  // "28,5 kph"
QString meters(std::optional<double> meters);  // "7 m"
QString bpm(std::optional<int> beatsPerMinute);  // "128 bpm"
QString number(std::optional<int> value);

struct Metric {
    QString value;
    QString label;
};

// Columns for an activity: distance sports get distance/time/pace or speed,
// others (cardio, strength) get time and heart rate. `detailed` adds more.
std::vector<Metric> summary(const fit::Totals& totals, bool detailed);

}  // namespace metrics
