#include <gtest/gtest.h>

#include "fit/analysis.hpp"

namespace {

using namespace std::chrono_literals;

fit::TrackPoint point(int second, std::optional<double> distance, std::optional<double> altitude,
                      std::optional<int> hr) {
    fit::TrackPoint p;
    p.time = fit::toTimestamp(1'000'000'000) + std::chrono::seconds{second};
    p.distanceM = distance;
    p.altitudeM = altitude;
    p.heartRateBpm = hr;
    return p;
}

TEST(Climb, IgnoresNoiseBelowThreshold) {
    const auto c = fit::climb({100, 101, 99.5, 100.8, 99.9, 101.2}, 3.0);
    EXPECT_DOUBLE_EQ(c.ascentM, 0.0);
    EXPECT_DOUBLE_EQ(c.descentM, 0.0);
}

TEST(Climb, CountsRealClimbsAndDescents) {
    const auto c = fit::climb({100, 105, 110, 104, 98, 103}, 3.0);
    EXPECT_DOUBLE_EQ(c.ascentM, 10.0 + 5.0);
    EXPECT_DOUBLE_EQ(c.descentM, 12.0);
}

TEST(MovingAverage, SmoothsAndShrinksAtEdges) {
    const auto out = fit::movingAverage({0, 3, 6, 9}, 1);
    ASSERT_EQ(out.size(), 4u);
    EXPECT_DOUBLE_EQ(out[0], 1.5);  // (0+3)/2
    EXPECT_DOUBLE_EQ(out[1], 3.0);  // (0+3+6)/3
    EXPECT_DOUBLE_EQ(out[3], 7.5);  // (6+9)/2
    EXPECT_TRUE(fit::movingAverage({}, 3).empty());
}

TEST(Percentile, NearestRank) {
    EXPECT_FALSE(fit::percentile({}, 0.5).has_value());
    EXPECT_DOUBLE_EQ(*fit::percentile({5, 1, 3, 2, 4}, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(*fit::percentile({5, 1, 3, 2, 4}, 0.5), 3.0);
    EXPECT_DOUBLE_EQ(*fit::percentile({5, 1, 3, 2, 4}, 1.0), 5.0);
}

TEST(Summarize, PrefersSessionValues) {
    fit::Activity a;
    fit::SessionSummary s;
    s.sport = 1;
    s.totalDistanceM = 5000;
    s.totalTimerS = 1500;
    s.avgSpeedMps = 3.4;
    s.totalAscentM = 42;
    s.avgHeartRate = 150;
    s.maxHeartRate = 170;
    a.sessions.push_back(s);
    a.points = {point(0, 0, 100, 120), point(10, 30, 200, 130)};

    const auto t = fit::summarize(a);
    EXPECT_EQ(t.sport, 1);
    EXPECT_DOUBLE_EQ(*t.distanceM, 5000);
    EXPECT_DOUBLE_EQ(*t.avgSpeedMps, 3.4);
    EXPECT_DOUBLE_EQ(*t.ascentM, 42);
    EXPECT_EQ(t.avgHeartRate, 150);
    EXPECT_EQ(t.start, a.points.front().time);  // no session start -> first point
}

TEST(Summarize, FallsBackToTrackPoints) {
    fit::Activity a;
    a.points = {point(0, 0, 100, 120), point(50, 180, 104, 140), point(100, 400, 110, std::nullopt)};

    const auto t = fit::summarize(a);
    EXPECT_FALSE(t.sport.has_value());
    EXPECT_DOUBLE_EQ(*t.distanceM, 400);
    EXPECT_DOUBLE_EQ(*t.timerS, 100);
    EXPECT_DOUBLE_EQ(*t.avgSpeedMps, 4.0);
    EXPECT_DOUBLE_EQ(*t.ascentM, 10);
    EXPECT_EQ(t.avgHeartRate, 130);
    EXPECT_EQ(t.maxHeartRate, 140);
    EXPECT_FALSE(t.calories.has_value());
}

fit::TrackPoint moving(int second, double distance, double speed, std::optional<double> cadence = {}) {
    fit::TrackPoint p;
    p.time = fit::toTimestamp(1'000'000'000) + std::chrono::seconds{second};
    p.distanceM = distance;
    p.speedMps = speed;
    p.cadence = cadence;
    return p;
}

TEST(MovingTime, ExcludesStandingStillAndPauses) {
    // 10 s samples: 0-100 s running, 110-130 s standing, paused until 430 s, 430-460 s running
    std::vector<fit::TrackPoint> points;
    for (int t = 0; t <= 100; t += 10) points.push_back(moving(t, t * 3.0, 3));
    for (int t = 110; t <= 130; t += 10) points.push_back(moving(t, 300, 0));
    for (int t = 430; t <= 460; t += 10) points.push_back(moving(t, 300 + (t - 430) * 3.0, 3));
    EXPECT_DOUBLE_EQ(*fit::movingTime(points), 100 + 30);  // the 300 s gap is a pause
}

TEST(MovingTime, NoSpeedDataIsUnknown) {
    fit::TrackPoint a, b;
    b.time = a.time + std::chrono::seconds{10};
    EXPECT_FALSE(fit::movingTime({a, b}).has_value());
}

TEST(Summarize, DerivedStatsFromTrack) {
    fit::Activity a;
    fit::SessionSummary s;
    s.sport = 1;
    s.totalDistanceM = 1200;
    s.totalTimerS = 400;
    s.totalElapsedS = 420;
    a.sessions.push_back(s);
    for (int i = 0; i <= 40; ++i) {  // 10 s samples, 3 m/s, 75 strides/min, then standing
        a.points.push_back(moving(i * 10, i * 30.0, i < 36 ? 3.0 : 0.0, i < 36 ? 75.0 : 0.0));
    }
    a.points[5].altitudeM = 100;
    a.points[20].altitudeM = 130;

    const auto t = fit::summarize(a);
    EXPECT_DOUBLE_EQ(*t.elapsedS, 420);
    EXPECT_DOUBLE_EQ(*t.timerS, 400);
    EXPECT_DOUBLE_EQ(*t.movingS, 350);  // 35 intervals of 10 s above the threshold
    EXPECT_NEAR(*t.avgMovingSpeedMps, 1200.0 / 350.0, 1e-9);
    EXPECT_DOUBLE_EQ(*t.minAltitudeM, 100);
    EXPECT_DOUBLE_EQ(*t.maxAltitudeM, 130);
    EXPECT_DOUBLE_EQ(*t.avgCadence, 75);  // zeros ignored
    EXPECT_DOUBLE_EQ(*t.maxCadence, 75);
    // steps = 75 strides/min * 2 * 350 s / 60 = 875 -> 1200 m / 875
    EXPECT_NEAR(*t.avgStepLengthM, 1200.0 / 875.0, 1e-9);
    EXPECT_NEAR(*t.maxSpeedMps, 3.0, 1e-9);
}

TEST(Summarize, SessionStatsWin) {
    fit::Activity a;
    fit::SessionSummary s;
    s.sport = 1;
    s.totalDistanceM = 1000;
    s.totalTimerS = 320;
    s.totalMovingS = 300;
    s.avgCadence = 76.5;
    s.avgStepLengthM = 1.23;
    s.minAltitudeM = 5;
    a.sessions.push_back(s);
    a.points = {moving(0, 0, 3, 80), moving(10, 30, 3, 80)};
    const auto t = fit::summarize(a);
    EXPECT_DOUBLE_EQ(*t.movingS, 300);
    EXPECT_DOUBLE_EQ(*t.avgCadence, 76.5);
    EXPECT_DOUBLE_EQ(*t.avgStepLengthM, 1.23);
    EXPECT_DOUBLE_EQ(*t.minAltitudeM, 5);
}

TEST(Summarize, EmptyActivity) {
    const auto t = fit::summarize(fit::Activity{});
    EXPECT_FALSE(t.start.has_value());
    EXPECT_FALSE(t.distanceM.has_value());
    EXPECT_FALSE(t.ascentM.has_value());
}

}  // namespace
