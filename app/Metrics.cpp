#include "Metrics.hpp"

#include <QLocale>

#include "Theme.hpp"
#include "fit/format.hpp"

namespace metrics {
namespace {
const QString kMissing = QStringLiteral("--");
}

QString distance(std::optional<double> m) {
    return m ? QStringLiteral("%1 km").arg(QLocale().toString(*m / 1000.0, 'f', 2)) : kMissing;
}

QString duration(std::optional<double> s) {
    return s ? QString::fromStdString(fit::formatDuration(*s)) : kMissing;
}

QString pace(std::optional<double> mps) {
    return mps ? QString::fromStdString(fit::formatPace(*mps)) : kMissing;
}

QString speed(std::optional<double> mps) {
    return mps ? QStringLiteral("%1 kph").arg(QLocale().toString(*mps * 3.6, 'f', 1)) : kMissing;
}

QString meters(std::optional<double> m) {
    return m ? QStringLiteral("%1 m").arg(QLocale().toString(*m, 'f', 0)) : kMissing;
}

QString bpm(std::optional<int> v) { return v ? QStringLiteral("%1 bpm").arg(*v) : kMissing; }

QString number(std::optional<int> v) { return v ? QString::number(*v) : kMissing; }

std::vector<Metric> summary(const fit::Totals& t, bool detailed) {
    std::vector<Metric> out;
    if (theme::hasDistance(t.sport) && t.distanceM && *t.distanceM > 0) {
        out.push_back({distance(t.distanceM), QObject::tr("Distance")});
        out.push_back({duration(t.timerS), QObject::tr("Time")});
        if (theme::usesPace(t.sport)) {
            out.push_back({pace(t.avgSpeedMps), QObject::tr("Avg Pace")});
        } else {
            out.push_back({speed(t.avgSpeedMps), QObject::tr("Avg Speed")});
        }
        out.push_back({meters(t.ascentM), QObject::tr("Total Ascent")});
        if (detailed) out.push_back({number(t.calories), QObject::tr("Calories")});
        out.push_back({bpm(t.avgHeartRate), QObject::tr("Avg HR")});
        if (detailed) out.push_back({bpm(t.maxHeartRate), QObject::tr("Max HR")});
    } else {
        out.push_back({duration(t.timerS), QObject::tr("Total Time")});
        out.push_back({bpm(t.avgHeartRate), QObject::tr("Avg HR")});
        out.push_back({bpm(t.maxHeartRate), QObject::tr("Max HR")});
        out.push_back({number(t.calories), QObject::tr("Calories")});
    }
    return out;
}

}  // namespace metrics
