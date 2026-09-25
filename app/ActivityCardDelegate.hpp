#pragma once

#include <QStyledItemDelegate>

// Paints one activity as a Garmin Connect-style card: sport icon, date,
// title, then a row of metric columns.
class ActivityCardDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
