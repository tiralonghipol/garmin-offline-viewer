#pragma once

#include <QString>
#include <QWidget>
#include <optional>

class ChartWidget;
class QFrame;
class QHBoxLayout;
class QLabel;
class QScrollArea;
class QTableWidget;
class RouteMapWidget;

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
    void setHoverSeconds(std::optional<double> seconds);

    QString path_;
    QScrollArea* scroll_ = nullptr;
    QLabel* breadcrumb_ = nullptr;
    QLabel* icon_ = nullptr;
    QLabel* title_ = nullptr;
    QHBoxLayout* stats_ = nullptr;
    QFrame* mapCard_ = nullptr;
    RouteMapWidget* map_ = nullptr;
    ChartWidget* paceChart_ = nullptr;
    ChartWidget* heartRateChart_ = nullptr;
    ChartWidget* elevationChart_ = nullptr;
    QLabel* deviceName_ = nullptr;
    QLabel* deviceDetails_ = nullptr;
    QFrame* splitsCard_ = nullptr;
    QTableWidget* splits_ = nullptr;
};
