#include "fit/types.hpp"

#include <algorithm>
#include <bit>

namespace fit {
namespace {

bool isInteger(BaseType type) noexcept {
    switch (type) {
        case BaseType::Enum:
        case BaseType::SInt8:
        case BaseType::UInt8:
        case BaseType::SInt16:
        case BaseType::UInt16:
        case BaseType::SInt32:
        case BaseType::UInt32:
        case BaseType::UInt8z:
        case BaseType::UInt16z:
        case BaseType::UInt32z:
        case BaseType::Byte:
        case BaseType::SInt64:
        case BaseType::UInt64:
        case BaseType::UInt64z:
            return true;
        default:
            return false;
    }
}

bool isSigned(BaseType type) noexcept {
    return type == BaseType::SInt8 || type == BaseType::SInt16 || type == BaseType::SInt32 ||
           type == BaseType::SInt64;
}

// Each base type reserves one bit pattern meaning "no value".
std::uint64_t invalidPattern(BaseType type) noexcept {
    switch (type) {
        case BaseType::Enum:
        case BaseType::UInt8:
        case BaseType::Byte:
            return 0xFF;
        case BaseType::SInt8:
            return 0x7F;
        case BaseType::SInt16:
            return 0x7FFF;
        case BaseType::UInt16:
            return 0xFFFF;
        case BaseType::SInt32:
            return 0x7FFF'FFFF;
        case BaseType::UInt32:
        case BaseType::Float32:
            return 0xFFFF'FFFF;
        case BaseType::SInt64:
            return 0x7FFF'FFFF'FFFF'FFFF;
        case BaseType::UInt64:
        case BaseType::Float64:
            return ~std::uint64_t{0};
        default:  // the "z" types and strings use zero
            return 0;
    }
}

std::uint64_t loadLittleEndian(const std::vector<std::uint8_t>& raw) noexcept {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < raw.size() && i < 8; ++i) {
        value |= std::uint64_t{raw[i]} << (8 * i);
    }
    return value;
}

}  // namespace

std::size_t baseTypeSize(BaseType type) noexcept {
    switch (type) {
        case BaseType::Enum:
        case BaseType::SInt8:
        case BaseType::UInt8:
        case BaseType::String:
        case BaseType::UInt8z:
        case BaseType::Byte:
            return 1;
        case BaseType::SInt16:
        case BaseType::UInt16:
        case BaseType::UInt16z:
            return 2;
        case BaseType::SInt32:
        case BaseType::UInt32:
        case BaseType::Float32:
        case BaseType::UInt32z:
            return 4;
        case BaseType::Float64:
        case BaseType::SInt64:
        case BaseType::UInt64:
        case BaseType::UInt64z:
            return 8;
    }
    return 0;
}

std::optional<std::int64_t> Field::asInt() const {
    const std::size_t size = baseTypeSize(type);
    if (!isInteger(type) || size == 0 || raw.size() != size) {
        return std::nullopt;
    }
    std::uint64_t value = loadLittleEndian(raw);
    if (value == invalidPattern(type)) {
        return std::nullopt;
    }
    if (isSigned(type) && size < 8) {
        const std::uint64_t signBit = std::uint64_t{1} << (8 * size - 1);
        if ((value & signBit) != 0) {
            value |= ~((signBit << 1) - 1);  // sign-extend to 64 bits
        }
    }
    return static_cast<std::int64_t>(value);
}

std::optional<double> Field::asFloat() const {
    if (type == BaseType::Float32 && raw.size() == 4) {
        const auto bits = static_cast<std::uint32_t>(loadLittleEndian(raw));
        if (bits == invalidPattern(type)) return std::nullopt;
        return static_cast<double>(std::bit_cast<float>(bits));
    }
    if (type == BaseType::Float64 && raw.size() == 8) {
        const auto bits = loadLittleEndian(raw);
        if (bits == invalidPattern(type)) return std::nullopt;
        return std::bit_cast<double>(bits);
    }
    if (const auto value = asInt()) {
        return static_cast<double>(*value);
    }
    return std::nullopt;
}

std::optional<std::string> Field::asString() const {
    if (type != BaseType::String) return std::nullopt;
    const auto end = std::find(raw.begin(), raw.end(), std::uint8_t{0});
    if (end == raw.begin()) return std::nullopt;
    return std::string(raw.begin(), end);
}

const Field* Message::find(std::uint8_t number) const noexcept {
    const auto it = std::find_if(fields.begin(), fields.end(),
                                 [number](const Field& f) { return f.number == number; });
    return it == fields.end() ? nullptr : &*it;
}

std::optional<std::int64_t> Message::intValue(std::uint8_t number) const {
    const Field* field = find(number);
    return field ? field->asInt() : std::nullopt;
}

}  // namespace fit
