#pragma once

#include <QFutureWatcher>
#include <QMainWindow>

#include "ActivityListModel.hpp"

class ActivityDetailPage;
class ActivityListPage;
class QFileSystemWatcher;
class QLabel;
class QStackedWidget;

// Dark sidebar + two pages (activity list, activity detail), like Garmin Connect.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // A .fit file (opens it) or a folder / watch root (lists it).
    void openPath(const QString& path);

private:
    void loadFolder(const QString& folder);
    void showActivity(const QString& path);
    void showList();
    void openFolderDialog();
    void openFileDialog();
    void sendToGarminConnect(const QString& path);
    void checkWatch();
    [[nodiscard]] static QString watchRoot();  // /media/$USER/GARMIN

    QStackedWidget* pages_ = nullptr;
    ActivityListPage* listPage_ = nullptr;
    ActivityDetailPage* detailPage_ = nullptr;
    QLabel* watchStatus_ = nullptr;
    QFutureWatcher<FolderScan> scan_;
    QString currentFolder_;
    QFileSystemWatcher* mounts_ = nullptr;
    bool watchPresent_ = false;
};
