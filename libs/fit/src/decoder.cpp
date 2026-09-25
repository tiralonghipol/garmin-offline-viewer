#include "fit/decoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <string>

#include "fit/crc.hpp"

namespace fit {
namespace {

constexpr std::size_t kLegacyHeaderSize = 12;
constexpr std::size_t kHeaderSize = 14;
constexpr std::size_t kCrcSize = 2;
constexpr std::size_t kLocalMessageTypes = 16;

// Record header bits
constexpr std::uint8_t kCompressedTimestampBit = 0x80;
constexpr std::uint8_t kDefinitionBit = 0x40;
constexpr std::uint8_t kDeveloperDataBit = 0x20;
constexpr std::uint8_t kLocalTypeMask = 0x0F;

std::uint16_t readU16(std::span<const std::uint8_t> b, bool bigEndian = false) {
    return bigEndian ? static_cast<std::uint16_t>((b[0] << 8) | b[1])
                     : static_cast<std::uint16_t>(b[0] | (b[1] << 8));
}

std::uint32_t readU32(std::span<const std::uint8_t> b) {
    return std::uint32_t{b[0]} | (std::uint32_t{b[1]} << 8) | (std::uint32_t{b[2]} << 16) |
           (std::uint32_t{b[3]} << 24);
}

// Bounds-checked sequential reader over the record area.
class Cursor {
public:
    explicit Cursor(std::span<const std::uint8_t> data) : data_(data) {}

    [[nodiscard]] bool atEnd() const noexcept { return pos_ >= data_.size(); }

    std::uint8_t u8() { return take(1)[0]; }

    std::span<const std::uint8_t> take(std::size_t n) {
        if (data_.size() - pos_ < n) {
            throw ParseError("truncated record at data offset " + std::to_string(pos_));
        }
        const auto chunk = data_.subspan(pos_, n);
        pos_ += n;
        return chunk;
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

struct FieldDefinition {
    std::uint8_t number = 0;
    std::uint8_t size = 0;
    BaseType type = BaseType::Byte;
};

struct Definition {
    std::uint16_t globalNumber = 0;
    bool bigEndian = false;
    std::vector<FieldDefinition> fields;
    std::size_t developerDataSize = 0;  // skipped, we don't interpret developer fields
};

FileHeader parseHeader(std::span<const std::uint8_t> bytes, const DecodeOptions& options) {
    if (bytes.size() < kLegacyHeaderSize) {
        throw ParseError("file too small to be a FIT file");
    }
    FileHeader header;
    header.size = bytes[0];
    if (header.size != kLegacyHeaderSize && header.size != kHeaderSize) {
        throw ParseError("unsupported header size " + std::to_string(header.size));
    }
    if (bytes.size() < header.size) {
        throw ParseError("file truncated inside header");
    }
    header.protocolVersion = bytes[1];
    header.profileVersion = readU16(bytes.subspan(2, 2));
    header.dataSize = readU32(bytes.subspan(4, 4));

    constexpr std::array<std::uint8_t, 4> kSignature{'.', 'F', 'I', 'T'};
    if (!std::equal(kSignature.begin(), kSignature.end(), bytes.begin() + 8)) {
        throw ParseError("missing .FIT signature");
    }
    if (header.size == kHeaderSize) {
        header.crc = readU16(bytes.subspan(12, 2));
        // A header CRC of 0 means the writer chose not to compute it.
        if (options.verifyCrc && *header.crc != 0 &&
            crc16(bytes.first(kLegacyHeaderSize)) != *header.crc) {
            throw ParseError("header CRC mismatch");
        }
    }
    return header;
}

class RecordDecoder {
public:
    std::vector<Message> run(std::span<const std::uint8_t> records) {
        Cursor cursor(records);
        while (!cursor.atEnd()) {
            const std::uint8_t header = cursor.u8();
            if ((header & kCompressedTimestampBit) != 0) {
                // Compressed timestamp header: 2-bit local type + 5-bit time offset.
                const std::size_t local = (header >> 5) & 0x03u;
                const auto offset = static_cast<std::uint8_t>(header & 0x1Fu);
                const std::uint32_t timestamp = resolveCompressedTimestamp(offset);
                Message message = readData(cursor, local);
                setTimestamp(message, timestamp);
                messages_.push_back(std::move(message));
            } else if ((header & kDefinitionBit) != 0) {
                readDefinition(cursor, header & kLocalTypeMask,
                               (header & kDeveloperDataBit) != 0);
            } else {
                messages_.push_back(readData(cursor, header & kLocalTypeMask));
            }
        }
        return std::move(messages_);
    }

private:
    void readDefinition(Cursor& cursor, std::size_t local, bool hasDeveloperFields) {
        Definition def;
        cursor.take(1);  // reserved
        def.bigEndian = cursor.u8() != 0;
        def.globalNumber = readU16(cursor.take(2), def.bigEndian);
        const std::uint8_t fieldCount = cursor.u8();
        def.fields.reserve(fieldCount);
        for (std::uint8_t i = 0; i < fieldCount; ++i) {
            const auto f = cursor.take(3);
            def.fields.push_back({f[0], f[1], static_cast<BaseType>(f[2])});
        }
        if (hasDeveloperFields) {
            const std::uint8_t devCount = cursor.u8();
            for (std::uint8_t i = 0; i < devCount; ++i) {
                def.developerDataSize += cursor.take(3)[1];  // [number, size, dev index]
            }
        }
        definitions_[local] = std::move(def);
    }

    Message readData(Cursor& cursor, std::size_t local) {
        const auto& slot = definitions_[local];
        if (!slot) {
            throw ParseError("data message uses undefined local message type " +
                             std::to_string(local));
        }
        const Definition& def = *slot;

        Message message;
        message.globalNumber = def.globalNumber;
        message.fields.reserve(def.fields.size());
        for (const auto& fd : def.fields) {
            const auto bytes = cursor.take(fd.size);
            Field field{fd.number, fd.type, {bytes.begin(), bytes.end()}};

            // Normalise every element of the field to little-endian.
            const std::size_t elem = baseTypeSize(fd.type);
            if (def.bigEndian && elem > 1 && fd.size % elem == 0) {
                for (std::size_t i = 0; i < field.raw.size(); i += elem) {
                    const auto first = field.raw.begin() + static_cast<std::ptrdiff_t>(i);
                    std::reverse(first, first + static_cast<std::ptrdiff_t>(elem));
                }
            }
            if (fd.number == kTimestampField) {
                if (const auto ts = field.asInt()) {
                    lastTimestamp_ = static_cast<std::uint32_t>(*ts);
                }
            }
            message.fields.push_back(std::move(field));
        }
        cursor.take(def.developerDataSize);
        return message;
    }

    std::uint32_t resolveCompressedTimestamp(std::uint8_t offset) {
        if (!lastTimestamp_) {
            throw ParseError("compressed timestamp before any full timestamp");
        }
        // The offset holds the low 5 bits of the new timestamp; if it is
        // smaller than the previous low bits, the 32 s window rolled over.
        const std::uint32_t last = *lastTimestamp_;
        std::uint32_t timestamp = (last & ~0x1Fu) + offset;
        if (offset < (last & 0x1Fu)) {
            timestamp += 0x20;
        }
        lastTimestamp_ = timestamp;
        return timestamp;
    }

    static void setTimestamp(Message& message, std::uint32_t timestamp) {
        Field field{kTimestampField, BaseType::UInt32, {}};
        for (int shift = 0; shift < 32; shift += 8) {
            field.raw.push_back(static_cast<std::uint8_t>((timestamp >> shift) & 0xFFu));
        }
        const auto it = std::find_if(message.fields.begin(), message.fields.end(),
                                     [](const Field& f) { return f.number == kTimestampField; });
        if (it != message.fields.end()) {
            *it = std::move(field);
        } else {
            message.fields.push_back(std::move(field));
        }
    }

    std::array<std::optional<Definition>, kLocalMessageTypes> definitions_;
    std::optional<std::uint32_t> lastTimestamp_;
    std::vector<Message> messages_;
};

}  // namespace

DecodedFile decode(std::span<const std::uint8_t> bytes, const DecodeOptions& options) {
    DecodedFile out;
    out.header = parseHeader(bytes, options);

    const std::size_t dataEnd = std::size_t{out.header.size} + out.header.dataSize;
    if (bytes.size() < dataEnd + kCrcSize) {
        throw ParseError("file truncated: header announces " +
                         std::to_string(out.header.dataSize) + " data bytes");
    }
    if (options.verifyCrc) {
        const std::uint16_t expected = readU16(bytes.subspan(dataEnd, kCrcSize));
        if (crc16(bytes.first(dataEnd)) != expected) {
            throw ParseError("file CRC mismatch");
        }
    }
    out.messages = RecordDecoder{}.run(bytes.subspan(out.header.size, out.header.dataSize));
    return out;
}

DecodedFile decodeFile(const std::filesystem::path& path, const DecodeOptions& options) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw ParseError("cannot open " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(std::filesystem::file_size(path)));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in) {
        throw ParseError("failed to read " + path.string());
    }
    return decode(bytes, options);
}

}  // namespace fit
