#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

#include "Theme.hpp"
#include "fit/analysis.hpp"

// One row of the activity list: just the summary, not the full track, so a
// watch with hundreds of activities stays cheap to hold in memory.
struct ActivityEntry {
    QString path;
    QString title;
    std::optional<int> sport;
    QDateTime start;  // local time
    fit::Totals totals;
};
Q_DECLARE_METATYPE(ActivityEntry)

struct FolderScan {
    QString folder;
    std::vector<ActivityEntry> entries;  // newest first
    QStringList failures;                // files that could not be decoded
};

// Parses every .fit file in `folder`. Pure function, safe to run on a worker thread.
FolderScan scanFolder(const QString& folder);

// If `path` is a watch root or its GARMIN folder, returns the ACTIVITY folder inside.
QString resolveActivityFolder(const QString& path);

ActivityEntry makeEntry(const QString& path, const fit::Activity& activity);

class ActivityListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role { EntryRole = Qt::UserRole + 1, CategoryRole, SearchTextRole };

    using QAbstractListModel::QAbstractListModel;

    void setEntries(std::vector<ActivityEntry> entries);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    std::vector<ActivityEntry> entries_;
};

// Search box + sport buttons.
class ActivityFilterModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setCategory(std::optional<theme::Category> category);
    void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    std::optional<theme::Category> category_;
    QString search_;
};
