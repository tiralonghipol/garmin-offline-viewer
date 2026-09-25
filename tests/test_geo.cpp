#include <gtest/gtest.h>

#include "fit/geo.hpp"

namespace {

using fit::geo::LatLon;
using fit::geo::project;

TEST(Geo, ProjectsOriginToCentre) {
    const auto p = project({0.0, 0.0});
    EXPECT_DOUBLE_EQ(p.x, 0.5);
    EXPECT_NEAR(p.y, 0.5, 1e-12);
}

TEST(Geo, MatchesOsmTileNumbers) {
    // Trondheim cathedral (63.4269 N, 10.3969 E) is OSM tile 17/69321/35427
    // (checked against the formula on wiki.openstreetmap.org/wiki/Slippy_map_tilenames).
    const auto p = project({63.4269, 10.3969});
    EXPECT_EQ(static_cast<int>(p.x * (1 << 17)), 69321);
    EXPECT_EQ(static_cast<int>(p.y * (1 << 17)), 35427);
}

TEST(Geo, RoundTrips) {
    const LatLon in{63.4305, 10.3951};
    const auto out = fit::geo::unproject(project(in));
    EXPECT_NEAR(out.lat, in.lat, 1e-9);
    EXPECT_NEAR(out.lon, in.lon, 1e-9);
}

TEST(Geo, ClampsPoles) {
    EXPECT_GE(project({90.0, 0.0}).y, 0.0);
    EXPECT_LE(project({-90.0, 0.0}).y, 1.0);
}

TEST(Geo, ZoomToFit) {
    // A ~1.2 km run fits at street level; the whole world only at zoom 0.
    const auto a = project({63.405, 10.36});
    const auto b = project({63.415, 10.37});
    const int zoom = fit::geo::zoomToFit({a.x, b.y}, {b.x, a.y}, 800, 400);
    EXPECT_GE(zoom, 14);
    EXPECT_LE(zoom, 16);
    EXPECT_EQ(fit::geo::zoomToFit({0, 0}, {1, 1}, 800, 400), 0);
}

}  // namespace
