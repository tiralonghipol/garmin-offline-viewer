#include <gtest/gtest.h>

#include <limits>

#include "fit/format.hpp"

namespace {

TEST(Format, DurationUnderAnHour) {
    EXPECT_EQ(fit::formatDuration(0), "0:00");
    EXPECT_EQ(fit::formatDuration(59.4), "0:59");
    EXPECT_EQ(fit::formatDuration(59.6), "1:00");
    EXPECT_EQ(fit::formatDuration(3133), "52:13");
}

TEST(Format, DurationWithHours) { EXPECT_EQ(fit::formatDuration(3723), "1:02:03"); }

TEST(Format, DurationRejectsNonsense) {
    EXPECT_EQ(fit::formatDuration(-1), "--");
    EXPECT_EQ(fit::formatDuration(std::numeric_limits<double>::quiet_NaN()), "--");
}

TEST(Format, Pace) {
    EXPECT_EQ(fit::formatPace(1000.0 / 330.0), "5:30 /km");
    EXPECT_EQ(fit::formatPace(1000.0 / 239.6), "4:00 /km");
    EXPECT_EQ(fit::formatPace(0.0), "--");
}

TEST(Format, Utc) { EXPECT_EQ(fit::formatUtc(fit::toTimestamp(0)), "1989-12-31 00:00:00 UTC"); }

}  // namespace
