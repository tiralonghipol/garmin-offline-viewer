#include "ActivityListModel.hpp"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

ActivityEntry makeEntry(const QString& path, const fit::Activity& activity) {
    ActivityEntry e;
    e.path = path;
    e.totals = fit::summarize(activity);
    e.sport = e.totals.sport;
    if (e.totals.start) {
        e.start = QDateTime::fromSecsSinceEpoch(e.totals.start->time_since_epoch().count()).toLocalTime();
    } else {
        e.start = QFileInfo(path).lastModified();
    }
    e.title = QString::fromStdString(fit::activityTitle(e.sport, e.start.time().hour()));
    return e;
}

QString resolveActivityFolder(const QString& path) {
    QDir dir(path);
    for (const auto& sub : {QStringLiteral("GARMIN/ACTIVITY"), QStringLiteral("ACTIVITY")}) {
        if (dir.exists(sub)) return dir.filePath(sub);
    }
    return dir.absolutePath();
}

FolderScan scanFolder(const QString& folder) {
    FolderScan scan;
    scan.folder = resolveActivityFolder(folder);
    const auto files = QDir(scan.folder).entryInfoList({QStringLiteral("*.fit")}, QDir::Files);
    for (const QFileInfo& info : files) {
        try {
            const auto activity = fit::loadActivity(info.absoluteFilePath().toStdString());
            scan.entries.push_back(makeEntry(info.absoluteFilePath(), activity));
        } catch (const std::exception&) {
            scan.failures << info.fileName();
        }
    }
    std::sort(scan.entries.begin(), scan.entries.end(),
              [](const ActivityEntry& a, const ActivityEntry& b) { return a.start > b.start; });
    return scan;
}

void ActivityListModel::setEntries(std::vector<ActivityEntry> entries) {
    beginResetModel();
    entries_ = std::move(entries);
    endResetModel();
}

int ActivityListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant ActivityListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const ActivityEntry& e = entries_[static_cast<std::size_t>(index.row())];
    switch (role) {
        case Qt::DisplayRole: return e.title;
        case Qt::ToolTipRole: return QDir::toNativeSeparators(e.path);
        case EntryRole: return QVariant::fromValue(e);
        case CategoryRole: return static_cast<int>(theme::category(e.sport));
        case SearchTextRole:
            return QStringList{e.title, theme::sportLabel(e.sport),
                               QLocale().toString(e.start, QStringLiteral("d MMM yyyy")),
                               QFileInfo(e.path).fileName()}
                .join(' ');
        default: return {};
    }
}

void ActivityFilterModel::setCategory(std::optional<theme::Category> category) {
    category_ = category;
    invalidateFilter();
}

void ActivityFilterModel::setSearchText(const QString& text) {
    search_ = text.trimmed();
    invalidateFilter();
}

bool ActivityFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (category_ && index.data(ActivityListModel::CategoryRole).toInt() != static_cast<int>(*category_)) {
        return false;
    }
    return search_.isEmpty() ||
           index.data(ActivityListModel::SearchTextRole).toString().contains(search_, Qt::CaseInsensitive);
}
