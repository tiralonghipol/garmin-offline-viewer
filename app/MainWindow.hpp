#pragma once

#include <QMainWindow>

class QLabel;
class QListWidget;
class QTableView;
class TrackPointModel;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Opens a .fit file, or a folder (the watch root, GARMIN/ or ACTIVITY/).
    void openPath(const QString& path);

private:
    void openFileDialog();
    void openFolderDialog();
    void loadFolder(const QString& path);
    void loadFile(const QString& path);

    QListWidget* fileList_ = nullptr;
    QLabel* summary_ = nullptr;
    QTableView* table_ = nullptr;
    TrackPointModel* model_ = nullptr;
};
