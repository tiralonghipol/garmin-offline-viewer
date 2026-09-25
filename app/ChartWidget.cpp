#include "ChartWidget.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <array>
#include <cmath>

#include "Theme.hpp"
#include "fit/analysis.hpp"
#include "fit/format.hpp"

ChartWidget::ChartWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(170);
}

void ChartWidget::setData(Config config, std::vector<double> xSeconds, std::vector<double> y) {
    config_ = std::move(config);
    x_ = std::move(xSeconds);
    y_ = std::move(y);

    // Robust range: ignore the extreme 2% so a GPS glitch doesn't flatten the chart.
    std::vector<double> finite;
    std::copy_if(y_.begin(), y_.end(), std::back_inserter(finite), [](double v) { return std::isfinite(v); });
    yMin_ = fit::percentile(finite, 0.02).value_or(0.0);
    yMax_ = fit::percentile(finite, 0.98).value_or(1.0);
    const double span = std::max(yMax_ - yMin_, std::abs(yMax_) * 0.05 + 1e-6);
    const bool nonNegative = yMin_ >= 0.0;
    // Extra headroom at the top of the chart (the low end when inverted) for the labels.
    yMin_ -= span * (config_.invertY ? 0.25 : 0.1);
    yMax_ += span * (config_.invertY ? 0.1 : 0.25);
    if (nonNegative) yMin_ = std::max(yMin_, 0.0);
    update();
}

void ChartWidget::setHoverSeconds(std::optional<double> seconds) {
    if (hover_ == seconds) return;
    hover_ = seconds;
    update();
}

QRectF ChartWidget::plotRect() const { return QRectF(rect()).adjusted(56, 30, -14, -24); }

double ChartWidget::toPixelX(double seconds) const {
    const QRectF r = plotRect();
    const double end = x_.empty() ? 1.0 : std::max(x_.back(), 1.0);
    return r.left() + seconds / end * r.width();
}

double ChartWidget::toPixelY(double value) const {
    const QRectF r = plotRect();
    double f = (std::clamp(value, yMin_, yMax_) - yMin_) / (yMax_ - yMin_);
    if (config_.invertY) f = 1.0 - f;
    return r.bottom() - f * r.height();
}

std::optional<std::size_t> ChartWidget::nearestIndex(double seconds) const {
    if (x_.empty()) return std::nullopt;
    const auto it = std::lower_bound(x_.begin(), x_.end(), seconds);
    std::size_t i = static_cast<std::size_t>(it - x_.begin());
    if (i == x_.size()) --i;
    if (i > 0 && seconds - x_[i - 1] < x_[i] - seconds) --i;
    return i;
}

void ChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);
    const QRectF r = plotRect();

    // Title with coloured dot
    p.setPen(Qt::NoPen);
    p.setBrush(config_.color);
    p.drawEllipse(QPointF(10, 14), 5, 5);
    p.setPen(theme::kText);
    p.drawText(QPointF(22, 19), config_.title);

    if (x_.empty()) {
        p.setPen(theme::kMuted);
        p.drawText(r, Qt::AlignCenter, tr("No data"));
        return;
    }

    // Horizontal grid + y labels
    QFont small = font();
    small.setPointSizeF(font().pointSizeF() * 0.8);
    p.setFont(small);
    for (int i = 0; i <= 3; ++i) {
        const double value = yMin_ + (yMax_ - yMin_) * i / 3.0;
        const double y = toPixelY(value);
        p.setPen(QPen(QColor(0xee, 0xee, 0xee), 1));
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        p.setPen(theme::kMuted);
        p.drawText(QRectF(0, y - 8, r.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                   config_.formatY ? config_.formatY(value) : QString::number(value));
    }

    // X labels: pick a step giving roughly 6-10 ticks
    const double end = x_.back();
    constexpr std::array steps{30.0, 60.0, 120.0, 300.0, 600.0, 900.0, 1800.0, 3600.0, 7200.0};
    const double step = *std::find_if(steps.begin(), steps.end() - 1,
                                      [end](double s) { return end / s <= 10.0; });
    for (double t = step; t < end; t += step) {
        const double x = toPixelX(t);
        p.setPen(QPen(QColor(0xf2, 0xf2, 0xf2), 1));
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        p.setPen(theme::kMuted);
        p.drawText(QRectF(x - 30, r.bottom() + 4, 60, 16), Qt::AlignCenter,
                   QString::fromStdString(fit::formatDuration(t)));
    }

    // Filled area per run of valid samples
    QColor fill = config_.color;
    fill.setAlpha(110);
    std::size_t i = 0;
    while (i < y_.size()) {
        while (i < y_.size() && std::isnan(y_[i])) ++i;
        if (i >= y_.size()) break;
        QPainterPath line;
        const std::size_t first = i;
        line.moveTo(toPixelX(x_[i]), toPixelY(y_[i]));
        for (++i; i < y_.size() && !std::isnan(y_[i]); ++i) line.lineTo(toPixelX(x_[i]), toPixelY(y_[i]));
        QPainterPath area = line;
        area.lineTo(toPixelX(x_[i - 1]), r.bottom());
        area.lineTo(toPixelX(x_[first]), r.bottom());
        area.closeSubpath();
        p.fillPath(area, fill);
        p.strokePath(line, QPen(config_.color.darker(115), 1.3));
    }

    // Average
    if (config_.average) {
        const double y = toPixelY(*config_.average);
        p.setPen(QPen(QColor(0x55, 0x55, 0x55), 1, Qt::DashLine));
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        const QString text = tr("Avg: %1").arg(config_.formatY ? config_.formatY(*config_.average)
                                                               : QString::number(*config_.average));
        const double w = QFontMetricsF(small).horizontalAdvance(text) + 10;
        const QRectF box(r.right() - w, y - 18, w, 16);
        p.fillRect(box, QColor(255, 255, 255, 220));
        p.setPen(QColor(0x44, 0x44, 0x44));
        p.drawText(box, Qt::AlignCenter, text);
    }

    // Hover crosshair + value
    if (hover_) {
        if (const auto idx = nearestIndex(*hover_)) {
            const double x = toPixelX(x_[*idx]);
            p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1));
            p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
            if (!std::isnan(y_[*idx])) {
                const double y = toPixelY(y_[*idx]);
                p.setPen(QPen(Qt::white, 2));
                p.setBrush(config_.color.darker(120));
                p.drawEllipse(QPointF(x, y), 4.5, 4.5);
                const QString value = !std::isfinite(y_[*idx]) ? QStringLiteral("--")
                                      : config_.formatY    ? config_.formatY(y_[*idx])
                                                           : QString::number(y_[*idx]);
                const QString text = QStringLiteral("%1  ·  %2")
                                         .arg(value,
                                              QString::fromStdString(fit::formatDuration(x_[*idx])));
                const double w = QFontMetricsF(small).horizontalAdvance(text) + 14;
                QRectF box(std::min(x + 8, r.right() - w), r.top(), w, 20);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0x33, 0x33, 0x33, 220));
                p.drawRoundedRect(box, 3, 3);
                p.setPen(Qt::white);
                p.drawText(box, Qt::AlignCenter, text);
            }
        }
    }
}

void ChartWidget::mouseMoveEvent(QMouseEvent* event) {
    const QRectF r = plotRect();
    if (x_.empty() || !r.contains(event->position())) {
        emit hovered(std::nullopt);
        return;
    }
    emit hovered((event->position().x() - r.left()) / r.width() * x_.back());
}

void ChartWidget::leaveEvent(QEvent*) { emit hovered(std::nullopt); }
