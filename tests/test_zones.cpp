#include <gtest/gtest.h>

#include "fit/analysis.hpp"

namespace {

fit::TrackPoint hrPoint(int second, int bpm) {
    fit::TrackPoint p;
    p.time = fit::toTimestamp(1'000'000'000) + std::chrono::seconds{second};
    p.heartRateBpm = bpm;
    return p;
}

TEST(Zones, DefaultsArePercentOfMax) {
    const auto z = fit::zonesFromMaxHeartRate(200);
    EXPECT_EQ(z.lowerBpm, (std::array<int, 5>{100, 120, 140, 160, 180}));
    EXPECT_EQ(z.maxHeartRate, 200);
    EXPECT_FALSE(z.fromDevice);
}

TEST(Zones, GarminLayoutWithZoneZero) {
    fit::HeartRateSettings s;
    s.maxHeartRate = 190;
    s.zoneHighBpm = {94, 113, 132, 151, 170, 190};  // tops of zone 0..5
    const auto z = fit::zonesFromDevice(s);
    ASSERT_TRUE(z.has_value());
    EXPECT_EQ(z->lowerBpm, (std::array<int, 5>{95, 114, 133, 152, 171}));
    EXPECT_EQ(z->maxHeartRate, 190);
    EXPECT_TRUE(z->fromDevice);
}

TEST(Zones, WahooLayoutWithOpenTop) {
    fit::HeartRateSettings s;
    s.zoneHighBpm = {124, 144, 163, 182, 254};  // as in a real ELEMNT file
    const auto z = fit::zonesFromDevice(s);
    ASSERT_TRUE(z.has_value());
    EXPECT_EQ(z->lowerBpm, (std::array<int, 5>{0, 125, 145, 164, 183}));
    EXPECT_EQ(z->maxHeartRate, 0);  // 254 means "open-ended", not a real max
}

TEST(Zones, RejectsUnusableSettings) {
    EXPECT_FALSE(fit::zonesFromDevice({}).has_value());
    fit::HeartRateSettings unsorted;
    unsorted.zoneHighBpm = {150, 120, 130, 140, 160};
    EXPECT_FALSE(fit::zonesFromDevice(unsorted).has_value());
}

TEST(Zones, ZoneOfAndFractionalZone) {
    const auto z = fit::zonesFromMaxHeartRate(200);  // 100/120/140/160/180
    EXPECT_EQ(fit::zoneOf(90, z), 0);
    EXPECT_EQ(fit::zoneOf(100, z), 1);
    EXPECT_EQ(fit::zoneOf(139, z), 2);
    EXPECT_EQ(fit::zoneOf(195, z), 5);
    EXPECT_DOUBLE_EQ(fit::fractionalZone(130, z), 2.5);
    EXPECT_DOUBLE_EQ(fit::fractionalZone(190, z), 5.0 + 10.0 / 21.0);  // zone 5 ends at max+1
    EXPECT_LT(fit::fractionalZone(80, z), 1.0);
    EXPECT_LT(fit::fractionalZone(250, z), 6.0);  // capped below the next zone
}

TEST(Zones, TimeInZonesSkipsPauses) {
    const auto z = fit::zonesFromMaxHeartRate(200);
    // 10 s at 110 (Z1), 20 s at 150 (Z3), a 300 s pause, then 5 s at 185 (Z5)
    const std::vector<fit::TrackPoint> points{hrPoint(0, 110), hrPoint(10, 150), hrPoint(30, 150),
                                              hrPoint(330, 185), hrPoint(335, 185)};
    const auto t = fit::timeInZones(points, z);
    EXPECT_DOUBLE_EQ(t[0], 0);
    EXPECT_DOUBLE_EQ(t[1], 10);
    EXPECT_DOUBLE_EQ(t[3], 20);
    EXPECT_DOUBLE_EQ(t[5], 5);
}

}  // namespace
