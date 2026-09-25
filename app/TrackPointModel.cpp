#include "TrackPointModel.hpp"

#include <QDateTime>

#include "fit/format.hpp"

namespace {

template <typename T>
QVariant number(const std::optional<T>& value, int decimals = 0) {
    if (!value) return {};
    return QString::number(static_cast<double>(*value), 'f', decimals);
}

}  // namespace

void TrackPointModel::setPoints(std::vector<fit::TrackPoint> points) {
    beginResetModel();
    points_ = std::move(points);
    endResetModel();
}

int TrackPointModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(points_.size());
}

int TrackPointModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant TrackPointModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) return {};
    if (role == Qt::TextAlignmentRole) {
        return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
    }
    if (role != Qt::DisplayRole) return {};
    return display(points_[static_cast<std::size_t>(index.row())], index.column());
}

QVariant TrackPointModel::display(const fit::TrackPoint& p, int column) const {
    switch (column) {
        case Time:
            return QDateTime::fromSecsSinceEpoch(p.time.time_since_epoch().count())
                .toLocalTime()
                .toString(QStringLiteral("HH:mm:ss"));
        case Elapsed: {
            const auto elapsed = p.time - points_.front().time;
            return QString::fromStdString(fit::formatDuration(static_cast<double>(elapsed.count())));
        }
        case Distance:
            return p.distanceM ? number(std::optional{*p.distanceM / 1000.0}, 3) : QVariant{};
        case Pace:
            return p.speedMps ? QString::fromStdString(fit::formatPace(*p.speedMps)) : QVariant{};
        case HeartRate:
            return number(p.heartRateBpm);
        case Cadence:
            return number(p.cadence);
        case Altitude:
            return number(p.altitudeM, 1);
        case Latitude:
            return number(p.latitudeDeg, 6);
        case Longitude:
            return number(p.longitudeDeg, 6);
        default:
            return {};
    }
}

QVariant TrackPointModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
        case Time: return tr("Time");
        case Elapsed: return tr("Elapsed");
        case Distance: return tr("Distance (km)");
        case Pace: return tr("Pace");
        case HeartRate: return tr("HR (bpm)");
        case Cadence: return tr("Cadence (rpm)");
        case Altitude: return tr("Altitude (m)");
        case Latitude: return tr("Latitude");
        case Longitude: return tr("Longitude");
        default: return {};
    }
}
