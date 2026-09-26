#pragma once

#include <QCache>
#include <QPixmap>
#include <QSet>
#include <QWidget>
#include <optional>
#include <vector>

#include "fit/geo.hpp"

class QNetworkAccessManager;
class QNetworkReply;
class QToolButton;

// Slippy map: OpenStreetMap tiles with the route drawn on top, coloured by
// speed. Works offline too (route on a plain background).
class RouteMapWidget final : public QWidget {
    Q_OBJECT

public:
    struct Sample {
        double lat = 0.0;
        double lon = 0.0;
        double elapsedS = 0.0;
        double speedMps = 0.0;  // NaN if unknown
    };

    explicit RouteMapWidget(QWidget* parent = nullptr);

    void setTrack(std::vector<Sample> samples);
    void setHighlightSeconds(std::optional<double> seconds);
    [[nodiscard]] bool hasTrack() const { return !samples_.empty(); }

    QSize sizeHint() const override { return {800, 420}; }

public slots:
    void fitToTrack();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] double worldPixels() const;
    [[nodiscard]] QPointF toScreen(fit::geo::WorldPoint p) const;
    [[nodiscard]] fit::geo::WorldPoint toWorld(QPointF screen) const;
    // Changes the zoom level and re-centres the map on the start of the route.
    void setZoom(int zoom);
    void drawTiles(QPainter& p);
    void requestTile(int z, int x, int y);
    void onTileFinished(QNetworkReply* reply);

    std::vector<Sample> samples_;
    std::vector<fit::geo::WorldPoint> world_;
    std::vector<QColor> colors_;
    fit::geo::WorldPoint centre_{0.5, 0.5};
    int zoom_ = 3;
    std::optional<double> highlight_;

    QNetworkAccessManager* network_ = nullptr;
    QCache<QString, QPixmap> tiles_{512};  // in-memory; the disk cache sits behind the network
    QSet<QString> pending_;
    bool tilesFailing_ = false;

    bool dragging_ = false;
    QPointF dragStart_;
    fit::geo::WorldPoint dragCentre_;
};
