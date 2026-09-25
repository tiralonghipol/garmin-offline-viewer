#include "Theme.hpp"

#include <algorithm>
#include <array>

#include "fit/activity.hpp"

namespace theme {
namespace {
constexpr int kGeneric = 0, kRunning = 1, kCycling = 2, kSwimming = 5, kWalking = 11, kHiking = 17;
}

Category category(std::optional<int> sport) {
    if (sport == kRunning) return Category::Running;
    if (sport == kCycling) return Category::Cycling;
    return Category::Other;
}

bool hasDistance(std::optional<int> sport) {
    return sport == kRunning || sport == kCycling || sport == kWalking || sport == kHiking ||
           sport == kSwimming || sport == kGeneric;
}

bool usesPace(std::optional<int> sport) {
    return sport == kRunning || sport == kWalking || sport == kHiking;
}

QColor sportColor(std::optional<int> sport) {
    switch (category(sport)) {
        case Category::Running: return {0xf4, 0x7b, 0x20};
        case Category::Cycling: return {0x2e, 0xa6, 0x4a};
        case Category::Other: break;
    }
    if (sport == kSwimming) return {0x1e, 0x88, 0xe5};
    if (sport == kWalking || sport == kHiking) return {0x8d, 0x6e, 0x63};
    return {0xe5, 0x39, 0x35};
}

QString sportIcon(std::optional<int> sport) {
    if (!sport) return QStringLiteral("💪");
    switch (*sport) {
        case kRunning: return QStringLiteral("🏃");
        case kCycling: return QStringLiteral("🚴");
        case kSwimming: return QStringLiteral("🏊");
        case kWalking: return QStringLiteral("🚶");
        case kHiking: return QStringLiteral("🥾");
        default: return QStringLiteral("💪");
    }
}

QString sportLabel(std::optional<int> sport) {
    return sport ? QString::fromStdString(fit::sportName(*sport)).toUpper() : QStringLiteral("ACTIVITY");
}

QColor speedColor(double t) {
    struct Stop {
        double at;
        QColor color;
    };
    static const std::array<Stop, 5> stops{{
        {0.00, QColor(0x1e, 0x6f, 0xd9)},
        {0.35, QColor(0x2e, 0xb8, 0x4b)},
        {0.60, QColor(0xf2, 0xd0, 0x24)},
        {0.80, QColor(0xf5, 0x8a, 0x1f)},
        {1.00, QColor(0xe0, 0x2a, 0x2a)},
    }};
    t = std::clamp(t, 0.0, 1.0);
    for (std::size_t i = 1; i < stops.size(); ++i) {
        if (t <= stops[i].at) {
            const double f = (t - stops[i - 1].at) / (stops[i].at - stops[i - 1].at);
            const QColor& a = stops[i - 1].color;
            const QColor& b = stops[i].color;
            return QColor::fromRgbF(static_cast<float>(a.redF() + f * (b.redF() - a.redF())),
                                    static_cast<float>(a.greenF() + f * (b.greenF() - a.greenF())),
                                    static_cast<float>(a.blueF() + f * (b.blueF() - a.blueF())));
        }
    }
    return stops.back().color;
}

QString styleSheet() {
    return QStringLiteral(R"(
        QWidget#page, QWidget#pageContent { background: #f4f5f7; }
        QScrollArea { border: none; background: #f4f5f7; }

        QFrame#sidebar { background: #111111; }
        QLabel#brand { color: white; font-size: 24px; font-weight: 300; padding: 20px 18px 16px 18px; }
        QFrame#sidebar QPushButton {
            color: #d8d8d8; background: transparent; border: none; text-align: left;
            padding: 11px 18px; font-size: 14px; border-left: 3px solid transparent;
        }
        QFrame#sidebar QPushButton:hover { background: #222222; }
        QFrame#sidebar QPushButton:checked { background: #1f1f1f; color: white; border-left: 3px solid #11a9ed; }
        QLabel#watchStatus { color: #9a9a9a; padding: 14px 18px; font-size: 12px; }

        QLabel#pageTitle { font-size: 28px; font-weight: 300; color: #222222; }
        QLabel#activityTitle { font-size: 26px; font-weight: 300; color: #222222; }
        QLabel#breadcrumb { color: #777777; font-size: 11px; letter-spacing: 1px; }
        QLabel[role="muted"] { color: #777777; }
        QLabel[role="statValue"] { font-size: 24px; font-weight: 300; color: #222222; }
        QLabel[role="statLabel"] { color: #777777; font-size: 12px; }
        QLabel[role="cardTitle"] { font-size: 16px; color: #333333; }

        QLineEdit#search {
            border: 1px solid #c8c8c8; border-radius: 2px; padding: 6px 8px; background: white;
            min-width: 220px;
        }
        QPushButton[role="filter"] {
            border: 1px solid #c8c8c8; background: white; padding: 6px 20px; color: #333333;
            margin-left: -1px;
        }
        QPushButton[role="filter"]:checked { background: #1976d2; color: white; border-color: #1976d2; }
        QPushButton[role="primary"] {
            background: #1976d2; color: white; border: none; border-radius: 3px; padding: 8px 16px;
        }
        QPushButton[role="primary"]:hover { background: #1565c0; }
        QPushButton[role="primary"]:disabled { background: #9ec2e8; }
        QToolButton#back {
            border: 1px solid #b0b0b0; border-radius: 17px; min-width: 34px; min-height: 34px;
            max-width: 34px; max-height: 34px; font-size: 20px; color: #555555; background: white;
        }
        QToolButton#back:hover { background: #eeeeee; }

        QFrame#card { background: white; border: 1px solid #dddddd; border-radius: 4px; }
        QListView#activityList { background: transparent; border: none; outline: none; }

        QTableWidget#splits { border: none; background: white; gridline-color: #eeeeee; }
        QTableWidget#splits QHeaderView::section {
            background: white; border: none; border-bottom: 1px solid #dddddd; padding: 4px;
            color: #777777; font-size: 11px;
        }
    )");
}

}  // namespace theme
