#pragma once

#include <QString>
#include <QWidget>
#include <optional>
#include <vector>

#include "ChartWidget.hpp"
#include "fit/analysis.hpp"

class QFrame;
class QHBoxLayout;
class QLabel;
class QScrollArea;
class QTableWidget;
class RouteMapWidget;
class StatsPanel;
class ZonesPanel;

class ActivityDetailPage final : public QWidget {
    Q_OBJECT

public:
    explicit ActivityDetailPage(QWidget* parent = nullptr);

    // Loads and shows one activity; on failure returns false and fills `error`.
    bool showActivity(const QString& path, QString* error);
    [[nodiscard]] QString currentPath() const { return path_; }

signals:
    void backRequested();
    void sendRequested(const QString& path);

private:
    struct Series {
        ChartWidget* chart = nullptr;
        ChartWidget::Config config;
        std::vector<double> y;
    };

    void setHoverIndex(std::optional<std::size_t> index);
    void applyXAxis();
    void updateZones();
    void fillLaps();

    QString path_;
    fit::Activity activity_;
    fit::Totals totals_;
    std::vector<double> xTime_;      // seconds, pauses squeezed out
    std::vector<double> xDistance_;  // metres
    bool distanceAxis_ = false;
    std::vector<Series> series_;

    QScrollArea* scroll_ = nullptr;
    QLabel* breadcrumb_ = nullptr;
    QLabel* icon_ = nullptr;
    QLabel* title_ = nullptr;
    QHBoxLayout* stats_ = nullptr;
    QFrame* mapCard_ = nullptr;
    RouteMapWidget* map_ = nullptr;
    QWidget* axisToggle_ = nullptr;
    StatsPanel* statsPanel_ = nullptr;
    QTableWidget* laps_ = nullptr;
    ZonesPanel* zonesPanel_ = nullptr;
    QLabel* deviceName_ = nullptr;
    QLabel* deviceDetails_ = nullptr;
};
