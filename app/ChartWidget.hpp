#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <functional>
#include <optional>
#include <vector>

// Area chart over elapsed time, painted with QPainter (no QtCharts dependency).
// NaN values in y are gaps; +/-infinity means "off the scale" (e.g. pace while
// standing still): drawn clamped to the edge and ignored for the axis range. Hovering emits the time so the map and the other
// charts can show the same moment.
class ChartWidget final : public QWidget {
    Q_OBJECT

public:
    struct Config {
        QString title;
        QColor color;
        bool invertY = false;  // pace: faster (smaller) values at the top
        std::function<QString(double)> formatY;
        std::optional<double> average;
    };

    explicit ChartWidget(QWidget* parent = nullptr);

    void setData(Config config, std::vector<double> xSeconds, std::vector<double> y);
    void setHoverSeconds(std::optional<double> seconds);

    QSize sizeHint() const override { return {600, 190}; }

signals:
    void hovered(std::optional<double> seconds);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF plotRect() const;
    double toPixelX(double seconds) const;
    double toPixelY(double value) const;
    std::optional<std::size_t> nearestIndex(double seconds) const;

    Config config_;
    std::vector<double> x_;
    std::vector<double> y_;
    double yMin_ = 0.0, yMax_ = 1.0;
    std::optional<double> hover_;
};
