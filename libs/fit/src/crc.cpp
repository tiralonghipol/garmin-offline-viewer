#include "fit/crc.hpp"

#include <array>

namespace fit {
namespace {
// Nibble-wise lookup table from the FIT SDK.
constexpr std::array<std::uint16_t, 16> kCrcTable{
    0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
};
}  // namespace

std::uint16_t crcUpdate(std::uint16_t crc, std::uint8_t byte) noexcept {
    // Lower nibble
    std::uint16_t tmp = kCrcTable[crc & 0xFu];
    crc = static_cast<std::uint16_t>((crc >> 4) & 0x0FFFu);
    crc = static_cast<std::uint16_t>(crc ^ tmp ^ kCrcTable[byte & 0xFu]);
    // Upper nibble
    tmp = kCrcTable[crc & 0xFu];
    crc = static_cast<std::uint16_t>((crc >> 4) & 0x0FFFu);
    crc = static_cast<std::uint16_t>(crc ^ tmp ^ kCrcTable[(byte >> 4) & 0xFu]);
    return crc;
}

std::uint16_t crc16(std::span<const std::uint8_t> data, std::uint16_t crc) noexcept {
    for (const auto byte : data) {
        crc = crcUpdate(crc, byte);
    }
    return crc;
}

}  // namespace fit
