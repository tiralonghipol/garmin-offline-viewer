#include "ActivityDetailPage.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

#include "ChartWidget.hpp"
#include "Metrics.hpp"
#include "RouteMapWidget.hpp"
#include "Theme.hpp"
#include "fit/analysis.hpp"
#include "fit/format.hpp"

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kOffScale = std::numeric_limits<double>::infinity();
constexpr double kStandingStillMps = 0.5;  // slower than 33 min/km: no meaningful pace
constexpr double kPauseGapS = 60.0;         // longer recording gaps are pauses (auto-pause, stop button)

// "Slower ▬▬▬ Faster" legend under the map.
class SpeedLegend final : public QWidget {
public:
    using QWidget::QWidget;
    QSize sizeHint() const override { return {300, 26}; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRectF bar(width() / 2.0 - 60, height() / 2.0 - 4, 120, 8);
        QLinearGradient gradient(bar.topLeft(), bar.topRight());
        for (int i = 0; i <= 10; ++i) gradient.setColorAt(i / 10.0, theme::speedColor(i / 10.0));
        p.fillRect(bar, gradient);
        p.setPen(theme::kText);
        p.drawText(QRectF(0, 0, bar.left() - 8, height()), Qt::AlignRight | Qt::AlignVCenter, tr("Slower"));
        p.drawText(QRectF(bar.right() + 8, 0, 100, height()), Qt::AlignLeft | Qt::AlignVCenter, tr("Faster"));
    }
};

QFrame* card() {
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    return frame;
}

QLabel* label(const QString& text, const char* role) {
    auto* l = new QLabel(text);
    l->setProperty("role", role);
    return l;
}

QString relativeDay(const QDateTime& start) {
    const QDate today = QDate::currentDate();
    const QString time = start.toString(QStringLiteral("HH:mm"));
    if (start.date() == today) return QObject::tr("TODAY @ %1").arg(time);
    if (start.date() == today.addDays(-1)) return QObject::tr("YESTERDAY @ %1").arg(time);
    return QLocale().toString(start, QStringLiteral("ddd d MMM yyyy '@' HH:mm")).toUpper();
}

QTableWidgetItem* cell(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

}  // namespace

ActivityDetailPage::ActivityDetailPage(QWidget* parent) : QWidget(parent) {
    // --- header -----------------------------------------------------------
    auto* back = new QToolButton;
    back->setObjectName(QStringLiteral("back"));
    back->setText(QStringLiteral("‹"));
    back->setToolTip(tr("Back to activities (Esc)"));
    back->setCursor(Qt::PointingHandCursor);

    breadcrumb_ = new QLabel;
    breadcrumb_->setObjectName(QStringLiteral("breadcrumb"));
    icon_ = new QLabel;
    icon_->setAlignment(Qt::AlignCenter);
    icon_->setFixedSize(46, 46);
    title_ = new QLabel;
    title_->setObjectName(QStringLiteral("activityTitle"));

    auto* send = new QPushButton(QStringLiteral("☁  ") + tr("Send to Garmin Connect"));
    send->setProperty("role", "primary");
    send->setCursor(Qt::PointingHandCursor);
    send->setToolTip(tr("Open Garmin Connect's import page with this file ready to drop (Ctrl+U)"));

    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(icon_);
    titleRow->addSpacing(8);
    titleRow->addWidget(title_);
    titleRow->addStretch();
    auto* titleBlock = new QVBoxLayout;
    titleBlock->addWidget(breadcrumb_);
    titleBlock->addLayout(titleRow);

    auto* header = new QHBoxLayout;
    header->addWidget(back, 0, Qt::AlignTop);
    header->addSpacing(16);
    header->addLayout(titleBlock, 1);
    header->addWidget(send, 0, Qt::AlignTop);

    stats_ = new QHBoxLayout;
    stats_->setSpacing(48);

    // --- left column: map + charts ------------------------------------------
    mapCard_ = card();
    map_ = new RouteMapWidget;
    auto* mapLayout = new QVBoxLayout(mapCard_);
    mapLayout->setContentsMargins(1, 1, 1, 4);
    mapLayout->addWidget(map_, 1);
    mapLayout->addWidget(new SpeedLegend);
    mapCard_->setMinimumHeight(440);

    auto* chartsCard = card();
    auto* chartsLayout = new QVBoxLayout(chartsCard);
    chartsLayout->setContentsMargins(12, 12, 12, 12);
    chartsLayout->setSpacing(10);
    paceChart_ = new ChartWidget;
    heartRateChart_ = new ChartWidget;
    elevationChart_ = new ChartWidget;
    for (auto* chart : {paceChart_, heartRateChart_, elevationChart_}) {
        chartsLayout->addWidget(chart);
        connect(chart, &ChartWidget::hovered, this, &ActivityDetailPage::setHoverSeconds);
    }

    auto* left = new QVBoxLayout;
    left->setSpacing(16);
    left->addWidget(mapCard_);
    left->addWidget(chartsCard);
    left->addStretch();

    // --- right column: device + splits --------------------------------------
    auto* deviceCard = card();
    auto* deviceLayout = new QVBoxLayout(deviceCard);
    deviceLayout->setContentsMargins(16, 16, 16, 16);
    auto* watch = new QLabel(QStringLiteral("⌚"));
    watch->setAlignment(Qt::AlignCenter);
    watch->setStyleSheet(QStringLiteral("font-size: 64px;"));
    deviceName_ = new QLabel;
    deviceName_->setAlignment(Qt::AlignCenter);
    deviceName_->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 14px;"));
    deviceDetails_ = label({}, "muted");
    deviceDetails_->setAlignment(Qt::AlignCenter);
    deviceDetails_->setWordWrap(true);
    deviceLayout->addWidget(watch);
    deviceLayout->addWidget(deviceName_);
    deviceLayout->addWidget(deviceDetails_);

    splitsCard_ = card();
    auto* splitsLayout = new QVBoxLayout(splitsCard_);
    splitsLayout->setContentsMargins(16, 14, 16, 12);
    splitsLayout->addWidget(label(tr("Splits"), "cardTitle"));
    splits_ = new QTableWidget;
    splits_->setObjectName(QStringLiteral("splits"));
    splits_->verticalHeader()->setVisible(false);
    splits_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    splits_->setSelectionMode(QAbstractItemView::NoSelection);
    splits_->setFocusPolicy(Qt::NoFocus);
    splits_->setShowGrid(false);
    splits_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    splits_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    splits_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    splitsLayout->addWidget(splits_);

    auto* rightHost = new QWidget;
    rightHost->setFixedWidth(340);
    auto* right = new QVBoxLayout(rightHost);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(16);
    right->addWidget(deviceCard);
    right->addWidget(splitsCard_);
    right->addStretch();

    auto* columns = new QHBoxLayout;
    columns->setSpacing(20);
    columns->addLayout(left, 1);
    columns->addWidget(rightHost, 0, Qt::AlignTop);

    // --- assemble inside a scroll area ----------------------------------------
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("pageContent"));
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 20, 28, 28);
    layout->setSpacing(18);
    layout->addLayout(header);
    layout->addLayout(stats_);
    layout->addLayout(columns);

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setWidget(content);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll_);

    connect(back, &QToolButton::clicked, this, &ActivityDetailPage::backRequested);
    connect(send, &QPushButton::clicked, this, [this] { emit sendRequested(path_); });
}

void ActivityDetailPage::setHoverSeconds(std::optional<double> seconds) {
    for (auto* chart : {paceChart_, heartRateChart_, elevationChart_}) chart->setHoverSeconds(seconds);
    map_->setHighlightSeconds(seconds);
}

bool ActivityDetailPage::showActivity(const QString& path, QString* error) {
    fit::Activity activity;
    try {
        activity = fit::loadActivity(path.toStdString());
    } catch (const std::exception& e) {
        if (error) *error = QString::fromUtf8(e.what());
        return false;
    }
    path_ = path;
    const fit::Totals totals = fit::summarize(activity);
    const QDateTime start = totals.start
        ? QDateTime::fromSecsSinceEpoch(totals.start->time_since_epoch().count()).toLocalTime()
        : QFileInfo(path).lastModified();

    // Header
    const QColor color = theme::sportColor(totals.sport);
    breadcrumb_->setText(QStringLiteral("%1  ·  %2").arg(theme::sportLabel(totals.sport), relativeDay(start)));
    title_->setText(QString::fromStdString(fit::activityTitle(totals.sport, start.time().hour())));
    icon_->setText(theme::sportIcon(totals.sport));
    icon_->setStyleSheet(QStringLiteral("background: %1; border: 2px solid %2; border-radius: 23px; font-size: 22px;")
                             .arg(color.lighter(185).name(), color.name()));

    // Stats row
    while (QLayoutItem* item = stats_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const auto& m : metrics::summary(totals, /*detailed=*/true)) {
        auto* block = new QWidget;
        auto* v = new QVBoxLayout(block);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        v->addWidget(label(m.value, "statValue"));
        v->addWidget(label(m.label, "statLabel"));
        stats_->addWidget(block);
    }
    stats_->addStretch();

    // Time series. x is elapsed time with pauses squeezed out, so the axis
    // roughly matches moving time as on Garmin Connect.
    const auto& points = activity.points;
    std::vector<double> x, speed, heartRate, altitude;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        const double dt = i == 0 ? 0.0 : static_cast<double>((p.time - points[i - 1].time).count());
        x.push_back(i == 0 ? 0.0 : x.back() + (dt > kPauseGapS ? 1.0 : dt));
        double v = p.speedMps.value_or(kNaN);
        if (!p.speedMps && i > 0 && dt > 0 && dt <= kPauseGapS && p.distanceM && points[i - 1].distanceM) {
            v = (*p.distanceM - *points[i - 1].distanceM) / dt;  // derive from distance
        }
        speed.push_back(v);
        heartRate.push_back(p.heartRateBpm ? *p.heartRateBpm : kNaN);
        altitude.push_back(p.altitudeM.value_or(kNaN));
    }
    // Smooth GPS speed jitter (NaN-aware: gaps are filled with 0 and stay slow).
    std::vector<double> filled(speed.size());
    std::transform(speed.begin(), speed.end(), filled.begin(), [](double v) { return std::isfinite(v) ? v : 0.0; });
    const std::vector<double> smooth = fit::movingAverage(filled, 4);

    const bool pace = theme::usesPace(totals.sport);
    std::vector<double> paceOrSpeed(smooth.size());
    for (std::size_t i = 0; i < smooth.size(); ++i) {
        const bool valid = std::isfinite(speed[i]) || smooth[i] > 0;
        if (pace) paceOrSpeed[i] = !valid ? kNaN : smooth[i] > kStandingStillMps ? 1000.0 / smooth[i] : kOffScale;
        else paceOrSpeed[i] = valid ? smooth[i] * 3.6 : kNaN;
    }
    const auto anyFinite = [](const std::vector<double>& v) {
        return std::any_of(v.begin(), v.end(), [](double d) { return std::isfinite(d); });
    };

    ChartWidget::Config paceConfig{
        .title = pace ? tr("Pace") : tr("Speed"),
        .color = theme::kPace,
        .invertY = pace,
        .formatY = pace ? std::function<QString(double)>([](double s) { return QString::fromStdString(fit::formatDuration(s)); })
                        : std::function<QString(double)>([](double k) { return QLocale().toString(k, 'f', 1); }),
        .average = totals.avgSpeedMps ? std::optional{pace ? 1000.0 / *totals.avgSpeedMps : *totals.avgSpeedMps * 3.6}
                                      : std::nullopt,
    };
    paceChart_->setData(paceConfig, x, paceOrSpeed);
    paceChart_->setVisible(anyFinite(paceOrSpeed));

    heartRateChart_->setData({.title = tr("Heart Rate"), .color = theme::kHeartRate, .invertY = false,
                              .formatY = [](double v) { return QStringLiteral("%1").arg(std::lround(v)); },
                              .average = totals.avgHeartRate ? std::optional<double>(*totals.avgHeartRate) : std::nullopt},
                             x, heartRate);
    heartRateChart_->setVisible(anyFinite(heartRate));

    elevationChart_->setData({.title = tr("Elevation"), .color = theme::kElevation, .invertY = false,
                              .formatY = [](double v) { return QStringLiteral("%1 m").arg(std::lround(v)); },
                              .average = std::nullopt},
                             x, fit::movingAverage(altitude, 2));
    elevationChart_->setVisible(anyFinite(altitude));

    // Map
    std::vector<RouteMapWidget::Sample> samples;
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (points[i].latitudeDeg && points[i].longitudeDeg) {
            samples.push_back({*points[i].latitudeDeg, *points[i].longitudeDeg, x[i], smooth[i]});
        }
    }
    mapCard_->setVisible(!samples.empty());
    map_->setTrack(std::move(samples));
    setHoverSeconds(std::nullopt);

    // Device
    const auto& file = activity.file;
    deviceName_->setText(QString::fromStdString(fit::productName(file.manufacturer, file.product)));
    QStringList details;
    if (file.softwareVersion) details << tr("Software: %1").arg(*file.softwareVersion, 0, 'f', 2);
    if (file.serialNumber) details << tr("Serial: %1").arg(*file.serialNumber);
    details << QFileInfo(path).fileName();
    deviceDetails_->setText(details.join('\n'));

    // Splits
    const auto& laps = activity.laps;
    splitsCard_->setVisible(laps.size() > 1);
    splits_->clear();
    splits_->setColumnCount(5);
    splits_->setHorizontalHeaderLabels({tr("Lap"), tr("Time"), tr("Distance"), pace ? tr("Pace /km") : tr("Speed"), tr("Avg HR")});
    splits_->setRowCount(static_cast<int>(laps.size()));
    for (std::size_t i = 0; i < laps.size(); ++i) {
        const auto& lap = laps[i];
        const int row = static_cast<int>(i);
        splits_->setItem(row, 0, cell(QString::number(row + 1)));
        splits_->setItem(row, 1, cell(metrics::duration(lap.totalTimerS)));
        splits_->setItem(row, 2, cell(metrics::distance(lap.totalDistanceM)));
        const QString lapPace = lap.avgSpeedMps && *lap.avgSpeedMps > 0
                                    ? QString::fromStdString(fit::formatDuration(1000.0 / *lap.avgSpeedMps))
                                    : QStringLiteral("--");  // header already says "Pace (/km)"
        splits_->setItem(row, 3, cell(pace ? lapPace : metrics::speed(lap.avgSpeedMps)));
        splits_->setItem(row, 4, cell(lap.avgHeartRate ? QString::number(*lap.avgHeartRate) : QStringLiteral("--")));
    }
    splits_->resizeRowsToContents();
    int height = splits_->horizontalHeader()->height() + 4;
    for (int r = 0; r < splits_->rowCount(); ++r) height += splits_->rowHeight(r);
    splits_->setFixedHeight(height);

    scroll_->verticalScrollBar()->setValue(0);
    return true;
}
