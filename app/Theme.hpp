#pragma once

#include <QColor>
#include <QString>
#include <optional>

// Colours, sport styling and the application style sheet (Garmin Connect-like).
namespace theme {

inline const QColor kAccent{0x19, 0x76, 0xd2};
inline const QColor kText{0x22, 0x22, 0x22};
inline const QColor kMuted{0x77, 0x77, 0x77};
inline const QColor kBorder{0xdd, 0xdd, 0xdd};
inline const QColor kHeartRate{0xe5, 0x39, 0x35};
inline const QColor kPace{0x42, 0x8b, 0xe8};
inline const QColor kElevation{0x5d, 0x9c, 0x59};

enum class Category { Running, Cycling, Other };

[[nodiscard]] Category category(std::optional<int> sport);
[[nodiscard]] bool hasDistance(std::optional<int> sport);  // running, cycling, walking, ...
[[nodiscard]] bool usesPace(std::optional<int> sport);     // min/km rather than km/h
[[nodiscard]] QColor sportColor(std::optional<int> sport);
[[nodiscard]] QString sportIcon(std::optional<int> sport);  // emoji
[[nodiscard]] QString sportLabel(std::optional<int> sport);  // "RUNNING"

// Slow (0) -> fast (1): blue, green, yellow, orange, red. Used by map and legend.
[[nodiscard]] QColor speedColor(double t);

[[nodiscard]] QString styleSheet();

}  // namespace theme
