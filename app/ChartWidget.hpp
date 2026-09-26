#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <functional>
#include <optional>
#include <vector>

// Area chart over time or distance, painted with QPainter (no QtCharts
// dependency). NaN values in y are gaps; +/-infinity means "off the scale"
// (e.g. pace while standing still): drawn clamped to the edge and ignored for
// the axis range. Hovering emits the sample index, so the map and the other
// charts can show the same moment whichever x-axis they use.
class ChartWidget final : public QWidget {
    Q_OBJECT

public:
    struct Config {
        QString title;
        QString unit;  // y-axis unit, e.g. "min/km", "bpm"; shown after the title
        QColor color;
        bool invertY = false;  // pace: faster (smaller) values at the top
        std::function<QString(double)> formatY;
        std::optional<double> average;
    };

    enum class XAxis { Time, Distance };  // x in seconds or in metres

    explicit ChartWidget(QWidget* parent = nullptr);

    void setData(Config config, std::vector<double> x, std::vector<double> y, XAxis axis);
    void setHoverIndex(std::optional<std::size_t> index);

    QSize sizeHint() const override { return {600, 190}; }

signals:
    void hovered(std::optional<std::size_t> index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF plotRect() const;
    double toPixelX(double x) const;
    double toPixelY(double value) const;
    std::optional<std::size_t> nearestIndex(double x) const;
    QString formatX(double x) const;  // tick label, without unit
    QString xUnit() const;            // "m:ss", "h:mm:ss" or "km"
    QString withUnit(const QString& value) const;

    Config config_;
    std::vector<double> x_;
    std::vector<double> y_;
    double yMin_ = 0.0, yMax_ = 1.0;
    XAxis axis_ = XAxis::Time;
    std::optional<std::size_t> hover_;
};
