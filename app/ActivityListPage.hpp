#pragma once

#include <QWidget>
#include <vector>

#include "ActivityListModel.hpp"

class QLabel;
class QListView;
class QStackedLayout;

class ActivityListPage final : public QWidget {
    Q_OBJECT

public:
    explicit ActivityListPage(QWidget* parent = nullptr);

    void setEntries(std::vector<ActivityEntry> entries, const QString& source);
    void setBusy(const QString& message);

signals:
    void activityActivated(const QString& path);
    void openFolderRequested();

private:
    ActivityListModel* model_ = nullptr;
    ActivityFilterModel* filter_ = nullptr;
    QListView* list_ = nullptr;
    QLabel* source_ = nullptr;
    QLabel* emptyText_ = nullptr;
    QStackedLayout* stack_ = nullptr;
};
