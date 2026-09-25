#include "StatsPanel.hpp"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <functional>

#include "Metrics.hpp"
#include "Theme.hpp"

namespace {

QLabel* label(const QString& text, const char* role) {
    auto* l = new QLabel(text);
    l->setProperty("role", role);
    return l;
}

// Segmented switch ("Pace | Speed"); calls `onPick(index)` when a button is clicked.
QWidget* toggle(const QStringList& options, int current, std::function<void(int)> onPick, QObject* owner) {
    auto* host = new QWidget;
    auto* row = new QHBoxLayout(host);
    row->setContentsMargins(0, 2, 0, 6);
    row->setSpacing(0);
    auto* group = new QButtonGroup(host);
    for (int i = 0; i < options.size(); ++i) {
        auto* b = new QPushButton(options[i]);
        b->setProperty("role", "toggle");
        b->setCheckable(true);
        b->setChecked(i == current);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b, i);
        row->addWidget(b);
    }
    row->addStretch();
    QObject::connect(group, &QButtonGroup::idClicked, owner, std::move(onPick));
    return host;
}

// One "section" of a column: underlined title, optional switch, value/caption pairs.
class Section {
public:
    explicit Section(QVBoxLayout* column, const QString& title) : column_(column) {
        column_->addWidget(label(title, "sectionTitle"));
        column_->addSpacing(8);
    }
    Section& widget(QWidget* w) {
        column_->addWidget(w);
        return *this;
    }
    Section& stat(const QString& value, const QString& caption) {
        column_->addWidget(label(value, "statBig"));
        column_->addWidget(label(caption, "statCaption"));
        column_->addSpacing(12);
        return *this;
    }
    ~Section() { column_->addSpacing(18); }

private:
    QVBoxLayout* column_;
};

}  // namespace

StatsPanel::StatsPanel(QWidget* parent) : QWidget(parent) {
    columns_ = new QHBoxLayout(this);
    columns_->setContentsMargins(0, 16, 0, 8);
    columns_->setSpacing(28);
}

void StatsPanel::setData(const fit::Totals& totals, const fit::HeartRateZones& zones) {
    totals_ = totals;
    zones_ = zones;
    rebuild();
}

void StatsPanel::rebuild() {
    // Recreate everything: simplest correct way to reflect a toggle change.
    // Widgets go through deleteLater(): rebuild() runs inside a toggle button's
    // click signal, and deleting that button right away would be a use-after-free.
    const auto dispose = [](QWidget* w) {
        if (!w) return;
        w->hide();
        w->deleteLater();
    };
    while (QLayoutItem* item = columns_->takeAt(0)) {
        if (QLayout* l = item->layout()) {
            while (QLayoutItem* child = l->takeAt(0)) {
                dispose(child->widget());
                delete child;
            }
        }
        dispose(item->widget());
        delete item;
    }
    std::array<QVBoxLayout*, 4> column{};
    for (auto*& c : column) {
        c = new QVBoxLayout;
        c->setSpacing(0);
        columns_->addLayout(c, 1);
    }

    const auto& t = totals_;
    const bool running = theme::usesPace(t.sport);
    const bool distanceSport = theme::hasDistance(t.sport) && t.distanceM && *t.distanceM > 0;

    // --- column 1: distance, calories, heart rate
    if (distanceSport) Section(column[0], tr("Distance")).stat(metrics::distance(t.distanceM), tr("Distance"));
    Section(column[0], tr("Calories")).stat(metrics::number(t.calories), tr("Calories"));
    {
        Section hr(column[0], tr("Heart Rate"));
        hr.widget(toggle({tr("bpm"), tr("% of Max"), tr("Zones")}, static_cast<int>(heartRateMode_),
                         [this](int id) {
                             heartRateMode_ = static_cast<HeartRateMode>(id);
                             rebuild();
                         },
                         this));
        const auto format = [this](std::optional<int> bpm) -> QString {
            switch (heartRateMode_) {
                case HeartRateMode::Bpm:
                    return metrics::bpm(bpm);
                case HeartRateMode::PercentOfMax:
                    return bpm && zones_.maxHeartRate > 0
                               ? metrics::percent(static_cast<double>(*bpm) / zones_.maxHeartRate)
                               : QStringLiteral("--");
                case HeartRateMode::Zones:
                    return bpm ? metrics::decimal(fit::fractionalZone(*bpm, zones_), 1) : QStringLiteral("--");
            }
            return {};
        };
        hr.stat(format(t.avgHeartRate), tr("Avg HR")).stat(format(t.maxHeartRate), tr("Max HR"));
    }

    // --- column 2: timing, elevation
    Section(column[1], tr("Timing"))
        .stat(metrics::duration(t.timerS), tr("Time"))
        .stat(metrics::duration(t.movingS), tr("Moving Time"))
        .stat(metrics::duration(t.elapsedS), tr("Elapsed Time"));
    if (t.ascentM || t.minAltitudeM) {
        Section(column[1], tr("Elevation"))
            .stat(metrics::meters(t.ascentM), tr("Total Ascent"))
            .stat(metrics::meters(t.descentM), tr("Total Descent"))
            .stat(metrics::meters(t.minAltitudeM), tr("Min Elev"))
            .stat(metrics::meters(t.maxAltitudeM), tr("Max Elev"));
    }

    // --- column 3: pace or speed
    if (distanceSport) {
        Section ps(column[2], running ? tr("Pace/Speed") : tr("Speed"));
        if (running) {
            ps.widget(toggle({tr("Pace"), tr("Speed")}, showSpeed_ ? 1 : 0,
                             [this](int id) {
                                 showSpeed_ = id == 1;
                                 rebuild();
                             },
                             this));
        }
        if (running && !showSpeed_) {
            ps.stat(metrics::pace(t.avgSpeedMps), tr("Avg Pace"))
                .stat(metrics::pace(t.avgMovingSpeedMps), tr("Avg Moving Pace"))
                .stat(metrics::pace(t.maxSpeedMps), tr("Best Pace"));
        } else {
            ps.stat(metrics::speed(t.avgSpeedMps), tr("Avg Speed"))
                .stat(metrics::speed(t.avgMovingSpeedMps), tr("Avg Moving Speed"))
                .stat(metrics::speed(t.maxSpeedMps), tr("Max Speed"));
        }
    }

    // --- column 4: cadence ("Running Dynamics" on Garmin)
    if (t.avgCadence) {
        Section dyn(column[3], running ? tr("Running Dynamics") : tr("Cadence"));
        dyn.stat(metrics::cadence(t.avgCadence, running), running ? tr("Avg Run Cadence") : tr("Avg Cadence"))
            .stat(metrics::cadence(t.maxCadence, running), running ? tr("Max Run Cadence") : tr("Max Cadence"));
        if (running) dyn.stat(metrics::lengthMeters(t.avgStepLengthM), tr("Avg Stride Length"));
    }

    for (auto* c : column) c->addStretch();
}
