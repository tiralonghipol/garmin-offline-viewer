#include "RouteMapWidget.hpp"

#include <QDir>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "Theme.hpp"
#include "fit/analysis.hpp"

namespace {
constexpr double kTileSize = 256.0;
constexpr int kMinZoom = 2;
constexpr int kMaxZoom = 19;

QString tileKey(int z, int x, int y) { return QStringLiteral("%1/%2/%3").arg(z).arg(x).arg(y); }

QToolButton* mapButton(const QString& text, const QString& tip, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setFixedSize(30, 30);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background: white; border: 1px solid #c8c8c8; border-radius: 15px;"
        " font-size: 16px; color: #333; } QToolButton:hover { background: #f0f0f0; }"));
    return b;
}
}  // namespace

RouteMapWidget::RouteMapWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(320);
    setCursor(Qt::OpenHandCursor);

    network_ = new QNetworkAccessManager(this);
    auto* cache = new QNetworkDiskCache(this);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                             QStringLiteral("/tiles"));
    cache->setMaximumCacheSize(200LL * 1024 * 1024);
    network_->setCache(cache);
    connect(network_, &QNetworkAccessManager::finished, this, &RouteMapWidget::onTileFinished);

    auto* zoomIn = mapButton(QStringLiteral("+"), tr("Zoom in"), this);
    auto* zoomOut = mapButton(QStringLiteral("−"), tr("Zoom out"), this);
    auto* fit = mapButton(QStringLiteral("⤢"), tr("Fit route"), this);
    zoomIn->move(12, 12);
    zoomOut->move(48, 12);
    fit->move(84, 12);
    connect(zoomIn, &QToolButton::clicked, this, [this] { setZoom(zoom_ + 1); });
    connect(zoomOut, &QToolButton::clicked, this, [this] { setZoom(zoom_ - 1); });
    connect(fit, &QToolButton::clicked, this, &RouteMapWidget::fitToTrack);
}

void RouteMapWidget::setTrack(std::vector<Sample> samples) {
    samples_ = std::move(samples);
    world_.clear();
    colors_.clear();
    highlight_.reset();

    std::vector<double> speeds;
    for (const auto& s : samples_) {
        world_.push_back(fit::geo::project({s.lat, s.lon}));
        if (std::isfinite(s.speedMps)) speeds.push_back(s.speedMps);
    }
    // Map speed to colour between the 5th and 95th percentile of this activity.
    const double lo = fit::percentile(speeds, 0.05).value_or(0.0);
    const double hi = fit::percentile(speeds, 0.95).value_or(1.0);
    for (const auto& s : samples_) {
        const double t = std::isfinite(s.speedMps) && hi > lo ? (s.speedMps - lo) / (hi - lo) : 0.5;
        colors_.push_back(theme::speedColor(t));
    }
    fitToTrack();
}

void RouteMapWidget::setHighlightSeconds(std::optional<double> seconds) {
    highlight_ = seconds;
    update();
}

void RouteMapWidget::fitToTrack() {
    if (world_.empty()) return;
    fit::geo::WorldPoint min{1, 1}, max{0, 0};
    for (const auto& w : world_) {
        min = {std::min(min.x, w.x), std::min(min.y, w.y)};
        max = {std::max(max.x, w.x), std::max(max.y, w.y)};
    }
    centre_ = {(min.x + max.x) / 2, (min.y + max.y) / 2};
    zoom_ = std::clamp(fit::geo::zoomToFit(min, max, width() - 80.0, height() - 80.0, 17), kMinZoom, kMaxZoom);
    update();
}

double RouteMapWidget::worldPixels() const { return kTileSize * std::ldexp(1.0, zoom_); }

QPointF RouteMapWidget::toScreen(fit::geo::WorldPoint p) const {
    return {(p.x - centre_.x) * worldPixels() + width() / 2.0, (p.y - centre_.y) * worldPixels() + height() / 2.0};
}

fit::geo::WorldPoint RouteMapWidget::toWorld(QPointF s) const {
    return {centre_.x + (s.x() - width() / 2.0) / worldPixels(), centre_.y + (s.y() - height() / 2.0) / worldPixels()};
}

void RouteMapWidget::setZoom(int zoom) {
    zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
    // Zooming always brings the start marker back to the middle, so the path is
    // explored from where the activity began (also after panning away).
    if (!world_.empty()) centre_ = world_.front();
    update();
}

void RouteMapWidget::requestTile(int z, int x, int y) {
    const QString key = tileKey(z, x, y);
    if (pending_.contains(key) || pending_.size() > 24) return;
    pending_.insert(key);
    // OSM tile usage policy: identify the app and honour caching.
    QNetworkRequest request(QUrl(QStringLiteral("https://tile.openstreetmap.org/%1.png").arg(key)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("fit-viewer/0.1 (personal FIT file viewer)"));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::User, key);
    network_->get(request);
}

void RouteMapWidget::onTileFinished(QNetworkReply* reply) {
    reply->deleteLater();
    const QString key = reply->request().attribute(QNetworkRequest::User).toString();
    pending_.remove(key);
    auto pixmap = std::make_unique<QPixmap>();
    if (reply->error() != QNetworkReply::NoError || !pixmap->loadFromData(reply->readAll())) {
        tilesFailing_ = true;
        update();
        return;
    }
    tilesFailing_ = false;
    tiles_.insert(key, pixmap.release());
    update();
}

void RouteMapWidget::drawTiles(QPainter& p) {
    const int n = 1 << zoom_;
    const QPointF topLeft = toScreen({0, 0});  // screen position of world origin
    const int x0 = static_cast<int>(std::floor(-topLeft.x() / kTileSize));
    const int y0 = std::max(0, static_cast<int>(std::floor(-topLeft.y() / kTileSize)));
    const int x1 = static_cast<int>(std::floor((width() - topLeft.x()) / kTileSize));
    const int y1 = std::min(n - 1, static_cast<int>(std::floor((height() - topLeft.y()) / kTileSize)));

    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            const int wx = ((tx % n) + n) % n;  // wrap around the antimeridian
            const QRectF target(topLeft.x() + tx * kTileSize, topLeft.y() + ty * kTileSize, kTileSize, kTileSize);
            if (const QPixmap* tile = tiles_.object(tileKey(zoom_, wx, ty))) {
                p.drawPixmap(target, *tile, tile->rect());
                continue;
            }
            // Meanwhile, stretch the parent tile's quadrant so zooming doesn't flash grey.
            if (const QPixmap* parent = tiles_.object(tileKey(zoom_ - 1, wx / 2, ty / 2))) {
                const QRectF source((wx % 2) * kTileSize / 2, (ty % 2) * kTileSize / 2, kTileSize / 2, kTileSize / 2);
                p.drawPixmap(target, *parent, source);
            }
            requestTile(zoom_, wx, ty);
        }
    }
}

void RouteMapWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0xee, 0xee, 0xe8));
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    drawTiles(p);

    if (world_.empty()) {
        p.setPen(theme::kMuted);
        p.drawText(rect(), Qt::AlignCenter, tr("No GPS data in this activity"));
        return;
    }

    // Route: white casing, then speed-coloured segments
    QPainterPath route;
    route.moveTo(toScreen(world_.front()));
    for (std::size_t i = 1; i < world_.size(); ++i) route.lineTo(toScreen(world_[i]));
    p.strokePath(route, QPen(QColor(255, 255, 255, 220), 7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (std::size_t i = 1; i < world_.size(); ++i) {
        p.setPen(QPen(colors_[i], 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(toScreen(world_[i - 1]), toScreen(world_[i]));
    }

    // Start (green ▶) and finish (red ■) markers
    const auto marker = [&p](QPointF at, const QColor& color, bool start) {
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(color);
        p.drawEllipse(at, 11, 11);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        if (start) {
            p.drawPolygon(QPolygonF{at + QPointF(-3.5, -5.5), at + QPointF(-3.5, 5.5), at + QPointF(5.5, 0)});
        } else {
            p.drawRect(QRectF(at - QPointF(4, 4), QSizeF(8, 8)));
        }
    };
    marker(toScreen(world_.back()), QColor(0xd3, 0x2f, 0x2f), false);
    marker(toScreen(world_.front()), QColor(0x2e, 0x9d, 0x3f), true);

    // Position linked to the chart cursor
    if (highlight_) {
        const auto it = std::lower_bound(samples_.begin(), samples_.end(), *highlight_,
                                         [](const Sample& s, double t) { return s.elapsedS < t; });
        const std::size_t i = std::min<std::size_t>(static_cast<std::size_t>(it - samples_.begin()), samples_.size() - 1);
        p.setPen(QPen(Qt::white, 3));
        p.setBrush(QColor(0x22, 0x22, 0x22));
        p.drawEllipse(toScreen(world_[i]), 7, 7);
    }

    // Attribution (required by OSM) and offline note
    QFont small = font();
    small.setPointSizeF(font().pointSizeF() * 0.8);
    p.setFont(small);
    const QString attribution = tr("© OpenStreetMap contributors");
    const QRectF box(width() - QFontMetricsF(small).horizontalAdvance(attribution) - 12, height() - 18,
                     QFontMetricsF(small).horizontalAdvance(attribution) + 12, 18);
    p.fillRect(box, QColor(255, 255, 255, 200));
    p.setPen(QColor(0x33, 0x33, 0x33));
    p.drawText(box, Qt::AlignCenter, attribution);
    if (tilesFailing_ && tiles_.isEmpty()) {
        const QString note = tr("Map tiles unavailable (offline?) – showing route only");
        const QRectF noteBox(120, 14, QFontMetricsF(small).horizontalAdvance(note) + 16, 24);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 220));
        p.drawRoundedRect(noteBox, 12, 12);
        p.setPen(theme::kMuted);
        p.drawText(noteBox, Qt::AlignCenter, note);
    }
}

void RouteMapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!event->oldSize().isValid() || event->oldSize().width() <= 0) fitToTrack();
}

void RouteMapWidget::wheelEvent(QWheelEvent* event) {
    setZoom(zoom_ + (event->angleDelta().y() > 0 ? 1 : -1));
    event->accept();
}

void RouteMapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    dragging_ = true;
    dragStart_ = event->position();
    dragCentre_ = centre_;
    setCursor(Qt::ClosedHandCursor);
}

void RouteMapWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return;
    const QPointF delta = event->position() - dragStart_;
    centre_ = {dragCentre_.x - delta.x() / worldPixels(), dragCentre_.y - delta.y() / worldPixels()};
    update();
}

void RouteMapWidget::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
    setCursor(Qt::OpenHandCursor);
}
