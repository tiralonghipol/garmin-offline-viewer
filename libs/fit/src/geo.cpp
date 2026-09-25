#include "fit/geo.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fit::geo {

WorldPoint project(LatLon position) noexcept {
    const double lat = std::clamp(position.lat, -kMaxLatitude, kMaxLatitude) * std::numbers::pi / 180.0;
    return {
        .x = (position.lon + 180.0) / 360.0,
        .y = (1.0 - std::log(std::tan(lat) + 1.0 / std::cos(lat)) / std::numbers::pi) / 2.0,
    };
}

LatLon unproject(WorldPoint point) noexcept {
    const double n = std::numbers::pi * (1.0 - 2.0 * point.y);
    return {
        .lat = std::atan(std::sinh(n)) * 180.0 / std::numbers::pi,
        .lon = point.x * 360.0 - 180.0,
    };
}

int zoomToFit(WorldPoint min, WorldPoint max, double widthPx, double heightPx, int maxZoom,
              double tileSize) noexcept {
    const double spanX = std::max(max.x - min.x, 1e-12);
    const double spanY = std::max(max.y - min.y, 1e-12);
    for (int zoom = maxZoom; zoom > 0; --zoom) {
        const double worldPx = tileSize * std::ldexp(1.0, zoom);
        if (spanX * worldPx <= widthPx && spanY * worldPx <= heightPx) return zoom;
    }
    return 0;
}

}  // namespace fit::geo
