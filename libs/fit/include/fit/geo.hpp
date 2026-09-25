#pragma once

// Web Mercator ("slippy map") maths, as used by OpenStreetMap tiles.
namespace fit::geo {

inline constexpr double kMaxLatitude = 85.0511287798;  // where Web Mercator becomes square

// Normalised world coordinates: x and y in [0, 1), origin at the top-left
// (180°W, 85.05°N). Multiply by 256 * 2^zoom to get pixels at that zoom.
struct WorldPoint {
    double x = 0.0;
    double y = 0.0;
};

struct LatLon {
    double lat = 0.0;
    double lon = 0.0;
};

[[nodiscard]] WorldPoint project(LatLon position) noexcept;
[[nodiscard]] LatLon unproject(WorldPoint point) noexcept;

// Largest zoom (0..maxZoom) at which the box [min, max] fits in a viewport of
// the given size in pixels.
[[nodiscard]] int zoomToFit(WorldPoint min, WorldPoint max, double widthPx, double heightPx,
                            int maxZoom = 18, double tileSize = 256.0) noexcept;

}  // namespace fit::geo
