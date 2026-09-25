#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "fit/crc.hpp"

namespace {

std::vector<std::uint8_t> bytesOf(std::string_view s) { return {s.begin(), s.end()}; }

TEST(Crc, EmptyInputIsZero) { EXPECT_EQ(fit::crc16({}), 0); }

TEST(Crc, MatchesCrc16ArcCheckValue) {
    // "123456789" is the standard check string for CRC catalogues.
    EXPECT_EQ(fit::crc16(bytesOf("123456789")), 0xBB3D);
}

TEST(Crc, AppendingLittleEndianCrcYieldsZero) {
    auto data = bytesOf("Forerunner 35");
    const std::uint16_t crc = fit::crc16(data);
    data.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    data.push_back(static_cast<std::uint8_t>(crc >> 8));
    EXPECT_EQ(fit::crc16(data), 0);
}

TEST(Crc, IncrementalEqualsOneShot) {
    const auto data = bytesOf("incremental");
    std::uint16_t crc = 0;
    for (auto b : data) crc = fit::crcUpdate(crc, b);
    EXPECT_EQ(crc, fit::crc16(data));
}

}  // namespace
