#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "fit/types.hpp"

namespace fit {

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct FileHeader {
    std::uint8_t size = 0;  // 12 (legacy) or 14 bytes
    std::uint8_t protocolVersion = 0;
    std::uint16_t profileVersion = 0;
    std::uint32_t dataSize = 0;  // bytes of records between header and file CRC
    std::optional<std::uint16_t> crc;
};

struct DecodeOptions {
    bool verifyCrc = true;
};

struct DecodedFile {
    FileHeader header;
    std::vector<Message> messages;  // data messages only, in file order
};

// Low-level decoder: turns bytes into generic messages without interpreting
// what the fields mean. Throws ParseError on malformed input.
[[nodiscard]] DecodedFile decode(std::span<const std::uint8_t> bytes,
                                 const DecodeOptions& options = {});
[[nodiscard]] DecodedFile decodeFile(const std::filesystem::path& path,
                                     const DecodeOptions& options = {});

}  // namespace fit
