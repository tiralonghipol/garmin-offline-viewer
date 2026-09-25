#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fit {

// FIT base types. Bit 7 flags multi-byte types ("endian ability"),
// bits 0-4 are the type number.
enum class BaseType : std::uint8_t {
    Enum = 0x00,
    SInt8 = 0x01,
    UInt8 = 0x02,
    SInt16 = 0x83,
    UInt16 = 0x84,
    SInt32 = 0x85,
    UInt32 = 0x86,
    String = 0x07,
    Float32 = 0x88,
    Float64 = 0x89,
    UInt8z = 0x0A,
    UInt16z = 0x8B,
    UInt32z = 0x8C,
    Byte = 0x0D,
    SInt64 = 0x8E,
    UInt64 = 0x8F,
    UInt64z = 0x90,
};

// Size in bytes of one element of the type, or 0 for an unknown type.
[[nodiscard]] std::size_t baseTypeSize(BaseType type) noexcept;

// Global message numbers (a small subset of the FIT profile).
namespace mesg {
inline constexpr std::uint16_t kFileId = 0;
inline constexpr std::uint16_t kSession = 18;
inline constexpr std::uint16_t kLap = 19;
inline constexpr std::uint16_t kRecord = 20;
inline constexpr std::uint16_t kEvent = 21;
inline constexpr std::uint16_t kDeviceInfo = 23;
inline constexpr std::uint16_t kActivity = 34;
}  // namespace mesg

// Field 253 means "timestamp" in every message type.
inline constexpr std::uint8_t kTimestampField = 253;

struct Field {
    std::uint8_t number = 0;
    BaseType type = BaseType::Byte;
    std::vector<std::uint8_t> raw;  // always stored little-endian

    // Scalar integer value; nullopt if the field holds the type's "invalid"
    // marker, is an array, or is not an integer type.
    [[nodiscard]] std::optional<std::int64_t> asInt() const;
    // Float value for float types, otherwise the integer value as double.
    [[nodiscard]] std::optional<double> asFloat() const;
    [[nodiscard]] std::optional<std::string> asString() const;
};

struct Message {
    std::uint16_t globalNumber = 0;
    std::vector<Field> fields;

    [[nodiscard]] const Field* find(std::uint8_t number) const noexcept;
    [[nodiscard]] std::optional<std::int64_t> intValue(std::uint8_t number) const;
};

}  // namespace fit
