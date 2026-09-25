#pragma once

#include <cstdint>
#include <span>

namespace fit {

// CRC-16 as specified by the FIT protocol (identical to CRC-16/ARC).
// Used for both the optional header CRC and the trailing file CRC.
[[nodiscard]] std::uint16_t crcUpdate(std::uint16_t crc, std::uint8_t byte) noexcept;
[[nodiscard]] std::uint16_t crc16(std::span<const std::uint8_t> data,
                                  std::uint16_t crc = 0) noexcept;

}  // namespace fit
