#pragma once

#include <QWidget>

#include "fit/analysis.hpp"

class QHBoxLayout;

// The "Stats" tab: Distance, Calories, Heart Rate (bpm / % of max / zones),
// Timing, Elevation, Pace/Speed and cadence, in four columns like Garmin Connect.
class StatsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit StatsPanel(QWidget* parent = nullptr);

    void setData(const fit::Totals& totals, const fit::HeartRateZones& zones);

private:
    enum class HeartRateMode { Bpm, PercentOfMax, Zones };

    void rebuild();

    fit::Totals totals_;
    fit::HeartRateZones zones_;
    HeartRateMode heartRateMode_ = HeartRateMode::Bpm;
    bool showSpeed_ = false;
    QHBoxLayout* columns_ = nullptr;
};
