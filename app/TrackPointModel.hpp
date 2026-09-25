#pragma once

#include <QAbstractTableModel>
#include <vector>

#include "fit/activity.hpp"

// Exposes a vector of track points to a QTableView (Qt's model/view pattern).
class TrackPointModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { Time, Elapsed, Distance, Pace, HeartRate, Cadence, Altitude, Latitude, Longitude, ColumnCount };

    using QAbstractTableModel::QAbstractTableModel;

    void setPoints(std::vector<fit::TrackPoint> points);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QVariant display(const fit::TrackPoint& point, int column) const;

    std::vector<fit::TrackPoint> points_;
};
