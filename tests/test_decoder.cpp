#include <gtest/gtest.h>

#include "fit/decoder.hpp"
#include "fit_builder.hpp"

namespace {

using fit::BaseType;
using fit::decode;
using fit::DecodeOptions;
using fit::ParseError;
using fit::test::Bytes;
using fit::test::FitBuilder;
namespace mesg = fit::mesg;

constexpr std::uint8_t kHeartRate = 3;
constexpr std::uint8_t kLat = 0;
constexpr std::uint8_t kTs = fit::kTimestampField;

// A record definition (timestamp + heart rate) with one data message.
std::vector<std::uint8_t> simpleFile() {
    return FitBuilder{}
        .definition(0, mesg::kRecord, {{kTs, 4, BaseType::UInt32}, {kHeartRate, 1, BaseType::UInt8}})
        .data(0, Bytes{}.u32(1'000'000'000).u8(142))
        .build();
}

TEST(Decoder, ParsesHeaderOfEmptyFile) {
    const auto file = decode(FitBuilder{}.build());
    EXPECT_EQ(file.header.size, 14);
    EXPECT_EQ(file.header.protocolVersion, 0x20);
    EXPECT_EQ(file.header.profileVersion, 2132);
    EXPECT_EQ(file.header.dataSize, 0u);
    EXPECT_TRUE(file.header.crc.has_value());
    EXPECT_TRUE(file.messages.empty());
}

TEST(Decoder, AcceptsLegacy12ByteHeader) {
    const auto file = decode(FitBuilder{}.build(/*legacyHeader=*/true));
    EXPECT_EQ(file.header.size, 12);
    EXPECT_FALSE(file.header.crc.has_value());
}

TEST(Decoder, DecodesLittleEndianDataMessage) {
    const auto bytes = FitBuilder{}
        .definition(0, mesg::kRecord,
                    {{kTs, 4, BaseType::UInt32}, {kHeartRate, 1, BaseType::UInt8},
                     {kLat, 4, BaseType::SInt32}})
        .data(0, Bytes{}.u32(1'000'000'000).u8(150).s32(-123456))
        .build();

    const auto file = decode(bytes);
    ASSERT_EQ(file.messages.size(), 1u);
    const auto& m = file.messages[0];
    EXPECT_EQ(m.globalNumber, mesg::kRecord);
    EXPECT_EQ(m.intValue(kTs), 1'000'000'000);
    EXPECT_EQ(m.intValue(kHeartRate), 150);
    EXPECT_EQ(m.intValue(kLat), -123456);
}

TEST(Decoder, DecodesBigEndianDefinition) {
    const bool be = true;
    const auto bytes = FitBuilder{}
        .definition(0, mesg::kRecord, {{kTs, 4, BaseType::UInt32}, {kLat, 4, BaseType::SInt32}}, be)
        .data(0, Bytes{}.u32(0x01020304, be).s32(-2, be))
        .build();

    const auto file = decode(bytes);
    ASSERT_EQ(file.messages.size(), 1u);
    EXPECT_EQ(file.messages[0].globalNumber, mesg::kRecord);
    EXPECT_EQ(file.messages[0].intValue(kTs), 0x01020304);
    EXPECT_EQ(file.messages[0].intValue(kLat), -2);
}

TEST(Decoder, InvalidMarkersBecomeMissingValues) {
    const auto bytes = FitBuilder{}
        .definition(0, mesg::kRecord,
                    {{kTs, 4, BaseType::UInt32}, {kHeartRate, 1, BaseType::UInt8},
                     {kLat, 4, BaseType::SInt32}})
        .data(0, Bytes{}.u32(0xFFFFFFFF).u8(0xFF).s32(0x7FFFFFFF))
        .build();

    const auto file = decode(bytes);
    const auto& m = file.messages.at(0);
    EXPECT_FALSE(m.intValue(kTs).has_value());
    EXPECT_FALSE(m.intValue(kHeartRate).has_value());
    EXPECT_FALSE(m.intValue(kLat).has_value());
}

TEST(Decoder, LocalTypeCanBeRedefined) {
    const auto bytes = FitBuilder{}
        .definition(0, mesg::kRecord, {{kHeartRate, 1, BaseType::UInt8}})
        .data(0, Bytes{}.u8(100))
        .definition(0, mesg::kEvent, {{kTs, 4, BaseType::UInt32}})
        .data(0, Bytes{}.u32(42))
        .build();

    const auto file = decode(bytes);
    ASSERT_EQ(file.messages.size(), 2u);
    EXPECT_EQ(file.messages[0].globalNumber, mesg::kRecord);
    EXPECT_EQ(file.messages[1].globalNumber, mesg::kEvent);
    EXPECT_EQ(file.messages[1].intValue(kTs), 42);
}

TEST(Decoder, ResolvesCompressedTimestampsIncludingRollover) {
    const auto bytes = FitBuilder{}
        .definition(0, mesg::kRecord, {{kTs, 4, BaseType::UInt32}, {kHeartRate, 1, BaseType::UInt8}})
        .definition(1, mesg::kRecord, {{kHeartRate, 1, BaseType::UInt8}})
        .data(0, Bytes{}.u32(1000).u8(100))   // 1000 = 31*32 + 8
        .compressed(1, 10, Bytes{}.u8(101))   // 10 > 8   -> 992 + 10       = 1002
        .compressed(1, 2, Bytes{}.u8(102))    // 2  < 10  -> 992 + 2 + 32  = 1026
        .build();

    const auto file = decode(bytes);
    ASSERT_EQ(file.messages.size(), 3u);
    EXPECT_EQ(file.messages[0].intValue(kTs), 1000);
    EXPECT_EQ(file.messages[1].intValue(kTs), 1002);
    EXPECT_EQ(file.messages[2].intValue(kTs), 1026);
    EXPECT_EQ(file.messages[2].intValue(kHeartRate), 102);
}

TEST(Decoder, CompressedTimestampWithoutReferenceThrows) {
    const auto bytes = FitBuilder{}
        .definition(0, mesg::kRecord, {{kHeartRate, 1, BaseType::UInt8}})
        .compressed(0, 1, Bytes{}.u8(90))
        .build();
    EXPECT_THROW((void)decode(bytes), ParseError);
}

TEST(Decoder, SkipsDeveloperFieldBytes) {
    const auto bytes = FitBuilder{}
        .definition(0, mesg::kRecord, {{kHeartRate, 1, BaseType::UInt8}}, false,
                    /*developerBytes=*/3)
        .data(0, Bytes{}.u8(120).u8(0xAA).u8(0xBB).u8(0xCC))
        .data(0, Bytes{}.u8(121).u8(0xAA).u8(0xBB).u8(0xCC))
        .build();

    const auto file = decode(bytes);
    ASSERT_EQ(file.messages.size(), 2u);
    EXPECT_EQ(file.messages[1].intValue(kHeartRate), 121);
}

TEST(Decoder, DetectsCorruptedPayloadViaCrc) {
    auto bytes = simpleFile();
    bytes[bytes.size() - 3] ^= 0x01;  // flip a bit in the heart-rate byte

    EXPECT_THROW((void)decode(bytes), ParseError);
    // With verification off, the (now wrong) value is decoded.
    const auto file = decode(bytes, DecodeOptions{.verifyCrc = false});
    EXPECT_EQ(file.messages.at(0).intValue(kHeartRate), 143);
}

TEST(Decoder, RejectsMissingSignature) {
    auto bytes = simpleFile();
    bytes[9] = 'X';
    EXPECT_THROW((void)decode(bytes, DecodeOptions{.verifyCrc = false}), ParseError);
}

TEST(Decoder, RejectsTruncatedFile) {
    auto bytes = simpleFile();
    bytes.resize(bytes.size() - 4);
    EXPECT_THROW((void)decode(bytes), ParseError);
}

TEST(Decoder, RejectsTruncatedRecord) {
    // Definition says 4 bytes, data provides only 2.
    const auto bytes = FitBuilder{}
        .definition(0, mesg::kRecord, {{kTs, 4, BaseType::UInt32}})
        .data(0, Bytes{}.u16(1))
        .build();
    EXPECT_THROW((void)decode(bytes), ParseError);
}

TEST(Decoder, RejectsDataWithoutDefinition) {
    const auto bytes = FitBuilder{}.data(3, Bytes{}.u8(1)).build();
    EXPECT_THROW((void)decode(bytes), ParseError);
}

TEST(Decoder, DecodeFileReportsMissingFile) {
    EXPECT_THROW((void)fit::decodeFile("does/not/exist.fit"), ParseError);
}

}  // namespace
