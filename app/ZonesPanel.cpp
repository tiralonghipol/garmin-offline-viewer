#include "ZonesPanel.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSpinBox>
#include <QVBoxLayout>
#include <numeric>

#include "Metrics.hpp"
#include "Theme.hpp"

namespace {
const std::array<const char*, 6> kZoneNames{
    QT_TRANSLATE_NOOP("ZonesPanel", "Below zone 1"), QT_TRANSLATE_NOOP("ZonesPanel", "Warm Up"),
    QT_TRANSLATE_NOOP("ZonesPanel", "Easy"),         QT_TRANSLATE_NOOP("ZonesPanel", "Aerobic"),
    QT_TRANSLATE_NOOP("ZonesPanel", "Threshold"),    QT_TRANSLATE_NOOP("ZonesPanel", "Maximum"),
};
}

ZonesPanel::ZonesPanel(QWidget* parent) : QWidget(parent) {
    source_ = new QLabel;
    source_->setProperty("role", "muted");
    maxHeartRate_ = new QSpinBox;
    maxHeartRate_->setRange(120, 230);
    maxHeartRate_->setSuffix(tr(" bpm"));
    maxHeartRate_->setToolTip(tr("Your maximum heart rate; zones are 50/60/70/80/90 % of it"));

    maxHeartRateBox_ = new QWidget;
    auto* maxRow = new QHBoxLayout(maxHeartRateBox_);
    maxRow->setContentsMargins(0, 0, 0, 0);
    maxRow->addWidget(new QLabel(tr("Max HR")));
    maxRow->addWidget(maxHeartRate_);

    auto* header = new QHBoxLayout;
    header->addWidget(source_);
    header->addStretch();
    header->addWidget(maxHeartRateBox_);

    grid_ = new QGridLayout;
    grid_->setHorizontalSpacing(14);
    grid_->setVerticalSpacing(10);
    grid_->setColumnStretch(2, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 16, 0, 8);
    layout->addLayout(header);
    layout->addSpacing(8);
    layout->addLayout(grid_);
    layout->addStretch();

    connect(maxHeartRate_, &QSpinBox::editingFinished, this, [this] { emit maxHeartRateChanged(maxHeartRate_->value()); });
}

void ZonesPanel::setData(const fit::HeartRateZones& zones, const std::array<double, 6>& seconds) {
    source_->setText(zones.fromDevice ? tr("Zones stored by your watch in this activity")
                                      : tr("Default zones: 50/60/70/80/90 % of max HR"));
    maxHeartRateBox_->setVisible(!zones.fromDevice);
    if (!zones.fromDevice) {
        const QSignalBlocker block(maxHeartRate_);
        maxHeartRate_->setValue(zones.maxHeartRate);
    }

    while (QLayoutItem* item = grid_->takeAt(0)) {
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }

    const double total = std::accumulate(seconds.begin(), seconds.end(), 0.0);
    int row = 0;
    for (int zone = 5; zone >= 0; --zone) {
        const auto z = static_cast<std::size_t>(zone);
        if (zone == 0 && seconds[0] <= 0) continue;  // only show "below zone 1" if it happened

        QString range;
        if (zone == 0) {
            range = tr("< %1 bpm").arg(zones.lowerBpm[0]);
        } else if (zone == 5) {
            range = tr("> %1 bpm").arg(zones.lowerBpm[4] - 1);
        } else {
            range = tr("%1 – %2 bpm").arg(zones.lowerBpm[z - 1]).arg(zones.lowerBpm[z] - 1);
        }

        auto* name = new QLabel(zone == 0 ? tr(kZoneNames[0])
                                          : tr("<b>Zone %1</b>  %2").arg(zone).arg(tr(kZoneNames[z])));
        auto* bpm = new QLabel(range);
        bpm->setProperty("role", "muted");
        auto* bar = new QProgressBar;
        bar->setProperty("role", "zone");
        bar->setTextVisible(false);
        bar->setRange(0, 1000);
        bar->setValue(total > 0 ? static_cast<int>(seconds[z] / total * 1000) : 0);
        bar->setStyleSheet(QStringLiteral("QProgressBar::chunk { background: %1; border-radius: 2px; }")
                               .arg(theme::zoneColor(zone).name()));
        auto* time = new QLabel(QStringLiteral("<b>%1</b>").arg(metrics::duration(seconds[z])));
        time->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* pct = new QLabel(total > 0 ? metrics::percent(seconds[z] / total) : QStringLiteral("--"));
        pct->setProperty("role", "muted");
        pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pct->setMinimumWidth(44);

        grid_->addWidget(name, row, 0);
        grid_->addWidget(bpm, row, 1);
        grid_->addWidget(bar, row, 2);
        grid_->addWidget(time, row, 3);
        grid_->addWidget(pct, row, 4);
        ++row;
    }
}
