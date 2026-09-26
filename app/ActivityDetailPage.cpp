#include "ActivityDetailPage.hpp"

#include <QButtonGroup>
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
#include <QSettings>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

#include "Metrics.hpp"
#include "RouteMapWidget.hpp"
#include "StatsPanel.hpp"
#include "Theme.hpp"
#include "ZonesPanel.hpp"
#include "fit/format.hpp"

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kOffScale = std::numeric_limits<double>::infinity();
const QString kMaxHeartRateKey = QStringLiteral("heartRate/max");
constexpr int kDefaultMaxHeartRate = 190;

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

// A stacked widget as tall as its current page (QTabWidget/QStackedWidget
// normally size themselves to the tallest page, leaving gaps under short ones).
class CurrentPageStack final : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;
    QSize sizeHint() const override { return currentWidget() ? currentWidget()->sizeHint() : QSize(); }
    QSize minimumSizeHint() const override {
        return currentWidget() ? currentWidget()->minimumSizeHint() : QSize();
    }
};

QTableWidgetItem* cell(const QString& text, bool bold = false) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (bold) {
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
    }
    return item;
}

bool anyFinite(const std::vector<double>& v) {
    return std::any_of(v.begin(), v.end(), [](double d) { return !std::isnan(d); });
}

}  // namespace

ActivityDetailPage::ActivityDetailPage(QWidget* parent) : QWidget(parent) {
    // --- header -----------------------------------------------------------------
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

    // --- map ------------------------------------------------------------------------
    mapCard_ = card();
    map_ = new RouteMapWidget;
    auto* mapLayout = new QVBoxLayout(mapCard_);
    mapLayout->setContentsMargins(1, 1, 1, 4);
    mapLayout->addWidget(map_, 1);
    mapLayout->addWidget(new SpeedLegend);
    mapCard_->setMinimumHeight(440);

    // --- charts, with a Time | Distance switch --------------------------------------
    auto* chartsCard = card();
    auto* chartsLayout = new QVBoxLayout(chartsCard);
    chartsLayout->setContentsMargins(12, 10, 12, 12);
    chartsLayout->setSpacing(10);

    axisToggle_ = new QWidget;
    auto* axisRow = new QHBoxLayout(axisToggle_);
    axisRow->setContentsMargins(0, 0, 0, 0);
    axisRow->setSpacing(0);
    axisRow->addStretch();
    auto* axisGroup = new QButtonGroup(this);
    for (const auto& [text, id] : {std::pair{tr("Time"), 0}, std::pair{tr("Distance"), 1}}) {
        auto* b = new QPushButton(text);
        b->setProperty("role", "toggle");
        b->setCheckable(true);
        b->setChecked(id == 0);
        b->setCursor(Qt::PointingHandCursor);
        axisGroup->addButton(b, id);
        axisRow->addWidget(b);
    }
    chartsLayout->addWidget(axisToggle_);
    connect(axisGroup, &QButtonGroup::idClicked, this, [this](int id) {
        distanceAxis_ = id == 1;
        applyXAxis();
    });

    for (int i = 0; i < 4; ++i) {  // pace/speed, heart rate, elevation, cadence
        auto* chart = new ChartWidget;
        chartsLayout->addWidget(chart);
        connect(chart, &ChartWidget::hovered, this, &ActivityDetailPage::setHoverIndex);
        series_.push_back({chart, {}, {}});
    }

    // --- Stats / Laps / Time in Zones tabs ------------------------------------------
    auto* tabsCard = card();
    auto* tabsLayout = new QVBoxLayout(tabsCard);
    tabsLayout->setContentsMargins(16, 8, 16, 12);
    auto* tabBar = new QTabBar;
    tabBar->setObjectName(QStringLiteral("detailTabs"));
    tabBar->setExpanding(false);
    tabBar->setDrawBase(false);
    auto* separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(QStringLiteral("color: #dddddd;"));
    auto* pages = new CurrentPageStack;
    statsPanel_ = new StatsPanel;
    laps_ = new QTableWidget;
    laps_->setObjectName(QStringLiteral("laps"));
    laps_->verticalHeader()->setVisible(false);
    laps_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    laps_->setSelectionMode(QAbstractItemView::NoSelection);
    laps_->setFocusPolicy(Qt::NoFocus);
    laps_->setShowGrid(false);
    laps_->setAlternatingRowColors(true);
    laps_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    laps_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    laps_->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* lapsHost = new QWidget;
    auto* lapsLayout = new QVBoxLayout(lapsHost);
    lapsLayout->setContentsMargins(0, 12, 0, 0);
    lapsLayout->addWidget(laps_);
    lapsLayout->addStretch();
    zonesPanel_ = new ZonesPanel;
    for (const auto& [page, name] : {std::pair<QWidget*, QString>{statsPanel_, tr("Stats")},
                                     {lapsHost, tr("Laps")},
                                     {zonesPanel_, tr("Time in Zones")}}) {
        tabBar->addTab(name);
        pages->addWidget(page);
    }
    tabsLayout->setSpacing(0);
    tabsLayout->addWidget(tabBar);
    tabsLayout->addWidget(separator);
    tabsLayout->addWidget(pages);
    connect(tabBar, &QTabBar::currentChanged, pages, [pages](int index) {
        pages->setCurrentIndex(index);
        pages->updateGeometry();  // new size hint -> the card shrinks or grows
    });
    connect(zonesPanel_, &ZonesPanel::maxHeartRateChanged, this, [this](int bpm) {
        QSettings().setValue(kMaxHeartRateKey, bpm);
        updateZones();
    });

    auto* left = new QVBoxLayout;
    left->setSpacing(16);
    left->addWidget(mapCard_);
    left->addWidget(chartsCard);
    left->addWidget(tabsCard);
    left->addStretch();

    // --- right column: device ---------------------------------------------------------
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

    auto* rightHost = new QWidget;
    rightHost->setFixedWidth(300);
    auto* right = new QVBoxLayout(rightHost);
    right->setContentsMargins(0, 0, 0, 0);
    right->addWidget(deviceCard);
    right->addStretch();

    auto* columns = new QHBoxLayout;
    columns->setSpacing(20);
    columns->addLayout(left, 1);
    columns->addWidget(rightHost, 0, Qt::AlignTop);

    // --- assemble inside a scroll area -------------------------------------------------
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

void ActivityDetailPage::setHoverIndex(std::optional<std::size_t> index) {
    for (auto& s : series_) s.chart->setHoverIndex(index);
    map_->setHighlightSeconds(index && *index < xTime_.size() ? std::optional{xTime_[*index]} : std::nullopt);
}

void ActivityDetailPage::applyXAxis() {
    const auto axis = distanceAxis_ ? ChartWidget::XAxis::Distance : ChartWidget::XAxis::Time;
    for (auto& s : series_) {
        s.chart->setData(s.config, distanceAxis_ ? xDistance_ : xTime_, s.y, axis);
        s.chart->setVisible(anyFinite(s.y));
    }
}

void ActivityDetailPage::updateZones() {
    const int storedMax = QSettings().value(kMaxHeartRateKey, kDefaultMaxHeartRate).toInt();
    auto zones = fit::zonesFromDevice(activity_.heartRate).value_or(fit::zonesFromMaxHeartRate(storedMax));
    if (zones.maxHeartRate <= 0) zones.maxHeartRate = storedMax;  // e.g. open-ended device zones
    statsPanel_->setData(totals_, zones);
    zonesPanel_->setData(zones, fit::timeInZones(activity_.points, zones));
}

void ActivityDetailPage::fillLaps() {
    const bool pace = theme::usesPace(totals_.sport);
    const bool running = pace;
    const auto& laps = activity_.laps;
    laps_->clear();
    laps_->setColumnCount(10);
    laps_->setHorizontalHeaderLabels({tr("Laps"), tr("Time"), tr("Cumulative\nTime"), tr("Distance"),
                                      pace ? tr("Avg Pace") : tr("Avg Speed"), tr("Avg HR"), tr("Max HR"),
                                      tr("Total\nAscent"), tr("Total\nDescent"),
                                      running ? tr("Avg Run\nCadence") : tr("Avg\nCadence")});
    laps_->setRowCount(static_cast<int>(laps.size()) + 1);

    const auto paceOrSpeed = [pace](std::optional<double> mps) {
        return pace ? metrics::pace(mps) : metrics::speed(mps);
    };
    double cumulative = 0.0;
    for (std::size_t i = 0; i < laps.size(); ++i) {
        const auto& lap = laps[i];
        const int row = static_cast<int>(i);
        cumulative += lap.totalTimerS.value_or(0.0);
        laps_->setItem(row, 0, cell(QString::number(row + 1)));
        laps_->setItem(row, 1, cell(metrics::duration(lap.totalTimerS)));
        laps_->setItem(row, 2, cell(metrics::duration(cumulative)));
        laps_->setItem(row, 3, cell(metrics::distance(lap.totalDistanceM)));
        laps_->setItem(row, 4, cell(paceOrSpeed(lap.avgSpeedMps)));
        laps_->setItem(row, 5, cell(metrics::number(lap.avgHeartRate)));
        laps_->setItem(row, 6, cell(metrics::number(lap.maxHeartRate)));
        laps_->setItem(row, 7, cell(metrics::meters(lap.totalAscentM)));
        laps_->setItem(row, 8, cell(metrics::meters(lap.totalDescentM)));
        laps_->setItem(row, 9, cell(metrics::cadence(lap.avgCadence, running)));
    }
    // Summary row, like Garmin's
    const int s = static_cast<int>(laps.size());
    laps_->setItem(s, 0, cell(tr("Summary"), true));
    laps_->setItem(s, 1, cell(metrics::duration(totals_.timerS), true));
    laps_->setItem(s, 2, cell(metrics::duration(totals_.timerS), true));
    laps_->setItem(s, 3, cell(metrics::distance(totals_.distanceM), true));
    laps_->setItem(s, 4, cell(paceOrSpeed(totals_.avgSpeedMps), true));
    laps_->setItem(s, 5, cell(metrics::number(totals_.avgHeartRate), true));
    laps_->setItem(s, 6, cell(metrics::number(totals_.maxHeartRate), true));
    laps_->setItem(s, 7, cell(metrics::meters(totals_.ascentM), true));
    laps_->setItem(s, 8, cell(metrics::meters(totals_.descentM), true));
    laps_->setItem(s, 9, cell(metrics::cadence(totals_.avgCadence, running), true));

    laps_->resizeRowsToContents();
    int height = laps_->horizontalHeader()->sizeHint().height() + 4;
    for (int r = 0; r < laps_->rowCount(); ++r) height += laps_->rowHeight(r);
    laps_->setFixedHeight(height);
}

bool ActivityDetailPage::showActivity(const QString& path, QString* error) {
    try {
        activity_ = fit::loadActivity(path.toStdString());
    } catch (const std::exception& e) {
        if (error) *error = QString::fromUtf8(e.what());
        return false;
    }
    path_ = path;
    totals_ = fit::summarize(activity_);
    const QDateTime start = totals_.start
        ? QDateTime::fromSecsSinceEpoch(totals_.start->time_since_epoch().count()).toLocalTime()
        : QFileInfo(path).lastModified();

    // Header
    const QColor color = theme::sportColor(totals_.sport);
    breadcrumb_->setText(QStringLiteral("%1  ·  %2").arg(theme::sportLabel(totals_.sport), relativeDay(start)));
    title_->setText(QString::fromStdString(fit::activityTitle(totals_.sport, start.time().hour())));
    icon_->setText(theme::sportIcon(totals_.sport));
    icon_->setStyleSheet(QStringLiteral("background: %1; border: 2px solid %2; border-radius: 23px; font-size: 22px;")
                             .arg(color.lighter(185).name(), color.name()));

    // Stats row
    while (QLayoutItem* item = stats_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const auto& m : metrics::summary(totals_, /*detailed=*/true)) {
        auto* block = new QWidget;
        auto* v = new QVBoxLayout(block);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        v->addWidget(label(m.value, "statValue"));
        v->addWidget(label(m.label, "statLabel"));
        stats_->addWidget(block);
    }
    stats_->addStretch();

    // Time series. x-time is elapsed time with pauses squeezed out, so the axis
    // roughly matches moving time as on Garmin Connect; x-distance is carried
    // forward where a sample has no distance.
    const auto& points = activity_.points;
    xTime_.clear();
    xDistance_.clear();
    std::vector<double> speed, heartRate, altitude, cadence;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        const double dt = i == 0 ? 0.0 : static_cast<double>((p.time - points[i - 1].time).count());
        xTime_.push_back(i == 0 ? 0.0 : xTime_.back() + (dt > fit::kPauseGapS ? 1.0 : dt));
        xDistance_.push_back(p.distanceM ? std::max(*p.distanceM, xDistance_.empty() ? 0.0 : xDistance_.back())
                                         : (xDistance_.empty() ? 0.0 : xDistance_.back()));
        double v = p.speedMps.value_or(kNaN);
        if (!p.speedMps && i > 0 && dt > 0 && dt <= fit::kPauseGapS && p.distanceM && points[i - 1].distanceM) {
            v = (*p.distanceM - *points[i - 1].distanceM) / dt;  // derive from distance
        }
        speed.push_back(v);
        heartRate.push_back(p.heartRateBpm ? *p.heartRateBpm : kNaN);
        altitude.push_back(p.altitudeM.value_or(kNaN));
        cadence.push_back(p.cadence && *p.cadence > 0 ? *p.cadence : kNaN);
    }
    // Smooth GPS speed jitter (gaps count as 0 and stay slow).
    std::vector<double> filled(speed.size());
    std::transform(speed.begin(), speed.end(), filled.begin(), [](double v) { return std::isfinite(v) ? v : 0.0; });
    const std::vector<double> smooth = fit::movingAverage(filled, 4);

    const bool pace = theme::usesPace(totals_.sport);
    const bool running = pace;
    std::vector<double> paceOrSpeed(smooth.size());
    for (std::size_t i = 0; i < smooth.size(); ++i) {
        const bool valid = std::isfinite(speed[i]) || smooth[i] > 0;
        if (pace) paceOrSpeed[i] = !valid ? kNaN : smooth[i] > fit::kMovingThresholdMps ? 1000.0 / smooth[i] : kOffScale;
        else paceOrSpeed[i] = valid ? smooth[i] * 3.6 : kNaN;
    }
    std::vector<double> cadenceShown(cadence.size());
    std::transform(cadence.begin(), cadence.end(), cadenceShown.begin(),
                   [running](double c) { return running ? c * 2.0 : c; });  // steps/min when running

    series_[0].config = {
        .title = pace ? tr("Pace") : tr("Speed"),
        .unit = pace ? tr("min/km") : tr("km/h"),
        .color = theme::kPace,
        .invertY = pace,
        .formatY = pace ? std::function<QString(double)>([](double s) { return QString::fromStdString(fit::formatDuration(s)); })
                        : std::function<QString(double)>([](double k) { return QLocale().toString(k, 'f', 1); }),
        .average = totals_.avgSpeedMps ? std::optional{pace ? 1000.0 / *totals_.avgSpeedMps : *totals_.avgSpeedMps * 3.6}
                                       : std::nullopt,
    };
    series_[0].y = std::move(paceOrSpeed);
    series_[1].config = {.title = tr("Heart Rate"), .unit = tr("bpm"), .color = theme::kHeartRate, .invertY = false,
                         .formatY = [](double v) { return QStringLiteral("%1").arg(std::lround(v)); },
                         .average = totals_.avgHeartRate ? std::optional<double>(*totals_.avgHeartRate) : std::nullopt};
    series_[1].y = std::move(heartRate);
    series_[2].config = {.title = tr("Elevation"), .unit = tr("m"), .color = theme::kElevation, .invertY = false,
                         .formatY = [](double v) { return QStringLiteral("%1").arg(std::lround(v)); },
                         .average = std::nullopt};
    series_[2].y = fit::movingAverage(altitude, 2);
    if (!anyFinite(altitude)) std::fill(series_[2].y.begin(), series_[2].y.end(), kNaN);
    series_[3].config = {.title = running ? tr("Run Cadence") : tr("Cadence"),
                         .unit = running ? tr("spm") : tr("rpm"),
                         .color = theme::kCadence,
                         .invertY = false,
                         .formatY = [](double v) { return QStringLiteral("%1").arg(std::lround(v)); },
                         .average = totals_.avgCadence ? std::optional{*totals_.avgCadence * (running ? 2.0 : 1.0)}
                                                       : std::nullopt};
    series_[3].y = std::move(cadenceShown);

    const bool hasDistance = !xDistance_.empty() && xDistance_.back() > 0;
    axisToggle_->setVisible(hasDistance);
    if (!hasDistance) distanceAxis_ = false;
    applyXAxis();

    // Map
    std::vector<RouteMapWidget::Sample> samples;
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (points[i].latitudeDeg && points[i].longitudeDeg) {
            samples.push_back({*points[i].latitudeDeg, *points[i].longitudeDeg, xTime_[i], smooth[i]});
        }
    }
    mapCard_->setVisible(!samples.empty());
    map_->setTrack(std::move(samples));
    setHoverIndex(std::nullopt);

    // Tabs
    updateZones();
    fillLaps();

    // Device
    const auto& file = activity_.file;
    deviceName_->setText(QString::fromStdString(fit::productName(file.manufacturer, file.product)));
    QStringList details;
    if (file.softwareVersion) details << tr("Software: %1").arg(*file.softwareVersion, 0, 'f', 2);
    if (file.serialNumber) details << tr("Serial: %1").arg(*file.serialNumber);
    details << QFileInfo(path).fileName();
    deviceDetails_->setText(details.join('\n'));

    scroll_->verticalScrollBar()->setValue(0);
    return true;
}
