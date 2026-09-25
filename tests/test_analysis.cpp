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

TEST(Summarize, EmptyActivity) {
    const auto t = fit::summarize(fit::Activity{});
    EXPECT_FALSE(t.start.has_value());
    EXPECT_FALSE(t.distanceM.has_value());
    EXPECT_FALSE(t.ascentM.has_value());
}

}  // namespace
