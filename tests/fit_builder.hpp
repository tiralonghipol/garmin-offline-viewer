#pragma once

// Test helpers that assemble FIT files byte by byte, so every decoder rule
// can be exercised without shipping binary fixtures.

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "fit/crc.hpp"
#include "fit/types.hpp"

namespace fit::test {

class Bytes {
public:
    Bytes& u8(std::uint8_t v) {
        data_.push_back(v);
        return *this;
    }
    Bytes& u16(std::uint16_t v, bool bigEndian = false) { return put(v, 2, bigEndian); }
    Bytes& u32(std::uint32_t v, bool bigEndian = false) { return put(v, 4, bigEndian); }
    Bytes& s32(std::int32_t v, bool bigEndian = false) {
        return u32(static_cast<std::uint32_t>(v), bigEndian);
    }
    Bytes& append(const std::vector<std::uint8_t>& other) {
        data_.insert(data_.end(), other.begin(), other.end());
        return *this;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& vec() const { return data_; }

private:
    Bytes& put(std::uint64_t v, std::size_t n, bool bigEndian) {
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t shift = 8 * (bigEndian ? n - 1 - i : i);
            data_.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
        }
        return *this;
    }
    std::vector<std::uint8_t> data_;
};

struct FieldSpec {
    std::uint8_t number;
    std::uint8_t size;
    BaseType type;
};

class FitBuilder {
public:
    FitBuilder& definition(std::uint8_t local, std::uint16_t global,
                           std::initializer_list<FieldSpec> fields, bool bigEndian = false,
                           std::uint8_t developerBytes = 0) {
        const std::uint8_t devFlag = developerBytes > 0 ? 0x20 : 0x00;
        records_.u8(static_cast<std::uint8_t>(0x40 | devFlag | (local & 0x0F)))
            .u8(0)  // reserved
            .u8(bigEndian ? 1 : 0)
            .u16(global, bigEndian)
            .u8(static_cast<std::uint8_t>(fields.size()));
        for (const auto& f : fields) {
            records_.u8(f.number).u8(f.size).u8(static_cast<std::uint8_t>(f.type));
        }
        if (developerBytes > 0) {
            records_.u8(1).u8(0).u8(developerBytes).u8(0);  // one dev field
        }
        return *this;
    }

    FitBuilder& data(std::uint8_t local, const Bytes& payload) {
        records_.u8(local & 0x0F).append(payload.vec());
        return *this;
    }

    FitBuilder& compressed(std::uint8_t local, std::uint8_t timeOffset, const Bytes& payload) {
        records_.u8(static_cast<std::uint8_t>(0x80 | ((local & 0x03) << 5) | (timeOffset & 0x1F)))
            .append(payload.vec());
        return *this;
    }

    [[nodiscard]] std::vector<std::uint8_t> build(bool legacyHeader = false) const {
        Bytes file;
        file.u8(legacyHeader ? 12 : 14)
            .u8(0x20)   // protocol 2.0
            .u16(2132)  // profile 21.32
            .u32(static_cast<std::uint32_t>(records_.vec().size()))
            .u8('.').u8('F').u8('I').u8('T');
        if (!legacyHeader) {
            file.u16(crc16(file.vec()));
        }
        file.append(records_.vec());
        file.u16(crc16(file.vec()));
        return file.vec();
    }

private:
    Bytes records_;
};

}  // namespace fit::test
