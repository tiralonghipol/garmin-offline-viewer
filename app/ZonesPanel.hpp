#pragma once

#include <QWidget>
#include <array>

#include "fit/analysis.hpp"

class QGridLayout;
class QLabel;
class QSpinBox;

// The "Time in Zones" tab: one bar per heart-rate zone, Z5 at the top.
class ZonesPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ZonesPanel(QWidget* parent = nullptr);

    void setData(const fit::HeartRateZones& zones, const std::array<double, 6>& secondsInZone);

signals:
    // Only offered when the file has no zones of its own.
    void maxHeartRateChanged(int bpm);

private:
    QLabel* source_ = nullptr;
    QWidget* maxHeartRateBox_ = nullptr;  // "Max HR [spin]", hidden when the watch provides zones
    QSpinBox* maxHeartRate_ = nullptr;
    QGridLayout* grid_ = nullptr;
};
