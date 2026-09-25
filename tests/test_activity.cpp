#include <gtest/gtest.h>

#include <cmath>

#include "fit/activity.hpp"
#include "fit_builder.hpp"

namespace {

using namespace std::chrono_literals;
using fit::BaseType;
using fit::test::Bytes;
using fit::test::FitBuilder;
namespace mesg = fit::mesg;

std::int32_t degreesToSemicircles(double deg) {
    return static_cast<std::int32_t>(std::lround(deg * (2147483648.0 / 180.0)));
}

TEST(Activity, FitEpochIsEndOf1989) {
    EXPECT_EQ(fit::toTimestamp(0).time_since_epoch(), 631065600s);
    EXPECT_EQ(fit::toTimestamp(86400) - fit::toTimestamp(0), 24h);
}

TEST(Activity, SemicirclesToDegrees) {
    EXPECT_DOUBLE_EQ(fit::semicirclesToDegrees(1 << 30), 90.0);
    EXPECT_DOUBLE_EQ(fit::semicirclesToDegrees(-(1 << 30)), -90.0);
    EXPECT_NEAR(fit::semicirclesToDegrees(degreesToSemicircles(63.4305)), 63.4305, 1e-6);
}

TEST(Activity, SportNames) {
    EXPECT_EQ(fit::sportName(1), "Running");
    EXPECT_EQ(fit::sportName(99), "Sport 99");
}

class ActivityFromFile : public ::testing::Test {
protected:
    void SetUp() override {
        const auto bytes =
            FitBuilder{}
                // file_id: type, manufacturer, product, serial, time_created
                .definition(0, mesg::kFileId,
                            {{0, 1, BaseType::Enum}, {1, 2, BaseType::UInt16},
                             {2, 2, BaseType::UInt16}, {3, 4, BaseType::UInt32z},
                             {4, 4, BaseType::UInt32}})
                .data(0, Bytes{}.u8(4).u16(1).u16(2503).u32(3'900'000'123).u32(1'100'000'000))
                // record: timestamp, lat, long, altitude, heart rate, distance, speed
                .definition(1, mesg::kRecord,
                            {{253, 4, BaseType::UInt32}, {0, 4, BaseType::SInt32},
                             {1, 4, BaseType::SInt32}, {2, 2, BaseType::UInt16},
                             {3, 1, BaseType::UInt8}, {5, 4, BaseType::UInt32},
                             {6, 2, BaseType::UInt16}})
                .data(1, Bytes{}
                             .u32(1'100'000'000)
                             .s32(degreesToSemicircles(63.4305))
                             .s32(degreesToSemicircles(10.3951))
                             .u16(2562)     // (12.4 m + 500) * 5
                             .u8(148)
                             .u32(123456)   // 1234.56 m
                             .u16(3200))    // 3.2 m/s
                // A record without timestamp must be dropped.
                .definition(2, mesg::kRecord, {{3, 1, BaseType::UInt8}})
                .data(2, Bytes{}.u8(150))
                // record using enhanced_altitude (uint32) instead of altitude
                .definition(3, mesg::kRecord,
                            {{253, 4, BaseType::UInt32}, {78, 4, BaseType::UInt32}})
                .data(3, Bytes{}.u32(1'100'000'001).u32(2600))  // 20 m
                // session summary
                .definition(4, mesg::kSession,
                            {{253, 4, BaseType::UInt32}, {2, 4, BaseType::UInt32},
                             {5, 1, BaseType::Enum}, {7, 4, BaseType::UInt32},
                             {9, 4, BaseType::UInt32}, {11, 2, BaseType::UInt16},
                             {16, 1, BaseType::UInt8}, {17, 1, BaseType::UInt8}})
                .data(4, Bytes{}
                             .u32(1'100'003'600)
                             .u32(1'100'000'000)
                             .u8(1)          // running
                             .u32(3'600'500) // 3600.5 s
                             .u32(1'002'000) // 10020 m
                             .u16(700)
                             .u8(150)
                             .u8(175))
                .build();
        activity_ = fit::toActivity(fit::decode(bytes));
    }

    fit::Activity activity_;
};

TEST_F(ActivityFromFile, FileInfo) {
    EXPECT_EQ(activity_.file.type, 4);
    EXPECT_EQ(activity_.file.manufacturer, 1);
    EXPECT_EQ(activity_.file.product, 2503);
    EXPECT_EQ(activity_.file.serialNumber, 3'900'000'123);
    EXPECT_EQ(activity_.file.timeCreated, fit::toTimestamp(1'100'000'000));
}

TEST_F(ActivityFromFile, TrackPointsAreScaled) {
    ASSERT_EQ(activity_.points.size(), 2u);
    const auto& p = activity_.points[0];
    EXPECT_EQ(p.time, fit::toTimestamp(1'100'000'000));
    EXPECT_NEAR(*p.latitudeDeg, 63.4305, 1e-6);
    EXPECT_NEAR(*p.longitudeDeg, 10.3951, 1e-6);
    EXPECT_NEAR(*p.altitudeM, 12.4, 1e-9);
    EXPECT_EQ(p.heartRateBpm, 148);
    EXPECT_DOUBLE_EQ(*p.distanceM, 1234.56);
    EXPECT_DOUBLE_EQ(*p.speedMps, 3.2);
    EXPECT_FALSE(p.cadence.has_value());
}

TEST_F(ActivityFromFile, EnhancedAltitudeIsUsed) {
    ASSERT_EQ(activity_.points.size(), 2u);
    EXPECT_DOUBLE_EQ(*activity_.points[1].altitudeM, 20.0);
    EXPECT_FALSE(activity_.points[1].latitudeDeg.has_value());
}

TEST_F(ActivityFromFile, SessionSummary) {
    ASSERT_EQ(activity_.sessions.size(), 1u);
    const auto& s = activity_.sessions[0];
    EXPECT_EQ(s.startTime, fit::toTimestamp(1'100'000'000));
    EXPECT_EQ(s.sport, 1);
    EXPECT_DOUBLE_EQ(*s.totalElapsedS, 3600.5);
    EXPECT_DOUBLE_EQ(*s.totalDistanceM, 10020.0);
    EXPECT_EQ(s.totalCalories, 700);
    EXPECT_EQ(s.avgHeartRate, 150);
    EXPECT_EQ(s.maxHeartRate, 175);
    EXPECT_FALSE(s.avgSpeedMps.has_value());
}

}  // namespace
