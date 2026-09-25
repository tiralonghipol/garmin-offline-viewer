#include "ActivityCardDelegate.hpp"

#include <QLocale>
#include <QPainter>
#include <QPainterPath>

#include "ActivityListModel.hpp"
#include "Metrics.hpp"
#include "Theme.hpp"

namespace {
constexpr int kCardHeight = 64;
constexpr int kGap = 8;

QFont scaled(QFont font, double factor, int weight = QFont::Normal) {
    font.setPointSizeF(font.pointSizeF() * factor);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}
}  // namespace

QSize ActivityCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    return {option.rect.width(), kCardHeight + kGap};
}

void ActivityCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
    const auto entry = index.data(ActivityListModel::EntryRole).value<ActivityEntry>();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Card
    const QRectF card = QRectF(option.rect).adjusted(1.5, kGap / 2.0, -1.5, -kGap / 2.0);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    painter->setPen(QPen(selected ? theme::kAccent : theme::kBorder, 1.0));
    painter->setBrush(hovered ? QColor(0xf7, 0xfa, 0xfd) : QColor(Qt::white));
    painter->drawRoundedRect(card, 4, 4);

    const double cy = card.center().y();
    double x = card.left() + 20;

    // Sport icon: tinted circle with coloured ring
    const QColor color = theme::sportColor(entry.sport);
    const QRectF circle(x, cy - 19, 38, 38);
    painter->setPen(QPen(color, 2.0));
    painter->setBrush(color.lighter(185));
    painter->drawEllipse(circle);
    painter->setFont(scaled(option.font, 1.35));
    painter->setPen(color.darker(130));
    painter->drawText(circle, Qt::AlignCenter, theme::sportIcon(entry.sport));
    x += 52;

    // Date: "25 Sep" over the year
    const QRectF dateRect(x, card.top() + 10, 58, 24);
    painter->setPen(theme::kText);
    painter->setFont(scaled(option.font, 1.2));
    painter->drawText(dateRect, Qt::AlignRight | Qt::AlignVCenter,
                      QLocale().toString(entry.start, QStringLiteral("d MMM")));
    painter->setPen(theme::kMuted);
    painter->setFont(scaled(option.font, 0.75));
    painter->drawText(QRectF(x, cy + 6, 58, 16), Qt::AlignRight | Qt::AlignVCenter,
                      QLocale().toString(entry.start, QStringLiteral("yyyy  HH:mm")));
    x += 78;

    // Title and sport
    const double titleWidth = std::max(160.0, card.width() * 0.26);
    painter->setPen(theme::kText);
    const QFont titleFont = scaled(option.font, 1.2);
    painter->setFont(titleFont);
    painter->drawText(QRectF(x, card.top() + 10, titleWidth, 24), Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(titleFont).elidedText(entry.title, Qt::ElideRight,
                                                         static_cast<int>(titleWidth)));
    painter->setPen(theme::kMuted);
    painter->setFont(scaled(option.font, 0.72, QFont::DemiBold));
    painter->drawText(QRectF(x, cy + 6, titleWidth, 16), Qt::AlignLeft | Qt::AlignVCenter,
                      theme::sportLabel(entry.sport));
    x += titleWidth + 16;

    // Metric columns
    const auto columns = metrics::summary(entry.totals, /*detailed=*/false);
    const double columnWidth = (card.right() - 16 - x) / static_cast<double>(columns.size());
    const QFont valueFont = scaled(option.font, 1.2);
    const QFont labelFont = scaled(option.font, 0.68, QFont::DemiBold);
    for (const auto& column : columns) {
        painter->setPen(theme::kAccent);
        painter->setFont(valueFont);
        painter->drawText(QRectF(x, card.top() + 10, columnWidth - 8, 24),
                          Qt::AlignLeft | Qt::AlignVCenter, column.value);
        painter->setPen(theme::kMuted);
        painter->setFont(labelFont);
        painter->drawText(QRectF(x, cy + 6, columnWidth - 8, 16), Qt::AlignLeft | Qt::AlignVCenter,
                          column.label.toUpper());
        x += columnWidth;
    }
    painter->restore();
}
