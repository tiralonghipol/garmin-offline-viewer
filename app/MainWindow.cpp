#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include "ActivityDetailPage.hpp"
#include "ActivityListPage.hpp"
#include "GarminConnect.hpp"

namespace {
const QString kLastFolderKey = QStringLiteral("lastFolder");

QPushButton* navButton(const QString& text) {
    auto* b = new QPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("FIT Viewer"));

    // --- sidebar ------------------------------------------------------------
    auto* sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(230);
    auto* brand = new QLabel(QStringLiteral("fit·viewer"));
    brand->setObjectName(QStringLiteral("brand"));
    auto* activities = navButton(QStringLiteral("🏃   ") + tr("Activities"));
    activities->setCheckable(true);
    activities->setChecked(true);
    auto* openWatch = navButton(QStringLiteral("📂   ") + tr("Open folder…"));
    auto* openFile = navButton(QStringLiteral("📄   ") + tr("Open file…"));
    watchStatus_ = new QLabel;
    watchStatus_->setObjectName(QStringLiteral("watchStatus"));
    watchStatus_->setWordWrap(true);

    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(0, 0, 0, 0);
    side->setSpacing(0);
    side->addWidget(brand);
    side->addWidget(activities);
    side->addSpacing(12);
    side->addWidget(openWatch);
    side->addWidget(openFile);
    auto* version = new QLabel(tr("Version %1").arg(QApplication::applicationVersion()));
    version->setObjectName(QStringLiteral("watchStatus"));
    version->setContentsMargins(0, 0, 0, 8);
    side->addStretch();
    side->addWidget(watchStatus_);
    side->addWidget(version);

    // --- pages ----------------------------------------------------------------
    listPage_ = new ActivityListPage;
    detailPage_ = new ActivityDetailPage;
    pages_ = new QStackedWidget;
    pages_->addWidget(listPage_);
    pages_->addWidget(detailPage_);

    auto* central = new QWidget;
    auto* row = new QHBoxLayout(central);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(sidebar);
    row->addWidget(pages_, 1);
    setCentralWidget(central);
    statusBar()->setSizeGripEnabled(false);

    // --- wiring -----------------------------------------------------------------
    connect(activities, &QPushButton::clicked, this, [this, activities] {
        activities->setChecked(true);
        showList();
    });
    connect(openWatch, &QPushButton::clicked, this, &MainWindow::openFolderDialog);
    connect(openFile, &QPushButton::clicked, this, &MainWindow::openFileDialog);
    connect(listPage_, &ActivityListPage::activityActivated, this, &MainWindow::showActivity);
    connect(listPage_, &ActivityListPage::openFolderRequested, this, &MainWindow::openFolderDialog);
    connect(detailPage_, &ActivityDetailPage::backRequested, this, &MainWindow::showList);
    connect(detailPage_, &ActivityDetailPage::sendRequested, this, &MainWindow::sendToGarminConnect);

    connect(&scan_, &QFutureWatcher<FolderScan>::finished, this, [this] {
        FolderScan result = scan_.result();
        currentFolder_ = result.folder;
        QSettings().setValue(kLastFolderKey, result.folder);
        const auto count = result.entries.size();
        listPage_->setEntries(std::move(result.entries), result.folder);
        QString message = tr("Loaded %n activities", nullptr, static_cast<int>(count));
        if (!result.failures.isEmpty()) {
            message += tr(" · %n file(s) could not be read: %1", nullptr, static_cast<int>(result.failures.size()))
                           .arg(result.failures.join(QStringLiteral(", ")));
        }
        statusBar()->showMessage(message, 8000);
    });

    // --- keyboard shortcuts -------------------------------------------------------
    const auto shortcut = [this](const QList<QKeySequence>& keys, auto slot) {
        auto* action = new QAction(this);
        action->setShortcuts(keys);
        connect(action, &QAction::triggered, this, slot);
        addAction(action);
    };
    shortcut({QKeySequence::Open}, &MainWindow::openFileDialog);
    shortcut({QKeySequence(tr("Ctrl+Shift+O"))}, &MainWindow::openFolderDialog);
    shortcut({QKeySequence(Qt::Key_Escape), QKeySequence::Back}, &MainWindow::showList);
    shortcut({QKeySequence(tr("Ctrl+U"))}, [this] {
        if (pages_->currentWidget() == detailPage_) sendToGarminConnect(detailPage_->currentPath());
    });
    shortcut({QKeySequence::Quit}, &QWidget::close);

    // --- watch detection: the FR35 mounts as /media/$USER/GARMIN on Ubuntu --------
    mounts_ = new QFileSystemWatcher(this);
    const QString mediaDir = QFileInfo(watchRoot()).absolutePath();
    if (QFileInfo::exists(mediaDir)) mounts_->addPath(mediaDir);
    connect(mounts_, &QFileSystemWatcher::directoryChanged, this, &MainWindow::checkWatch);

    watchPresent_ = QFileInfo::exists(watchRoot());
    checkWatch();
    if (watchPresent_) {
        loadFolder(watchRoot());
    } else if (const QString last = QSettings().value(kLastFolderKey).toString(); QFileInfo::exists(last)) {
        loadFolder(last);
    }
}

QString MainWindow::watchRoot() {
    return QStringLiteral("/media/%1/GARMIN").arg(qEnvironmentVariable("USER"));
}

void MainWindow::checkWatch() {
    const bool present = QFileInfo::exists(watchRoot());
    watchStatus_->setText(present ? QStringLiteral("⌚  ") + tr("Watch connected")
                                  : QStringLiteral("⌚  ") + tr("No watch connected"));
    if (present && !watchPresent_) {  // just plugged in
        statusBar()->showMessage(tr("Watch connected, loading activities…"), 4000);
        loadFolder(watchRoot());
    }
    watchPresent_ = present;
}

void MainWindow::openPath(const QString& path) {
    const QFileInfo info(path);
    if (info.isDir()) {
        loadFolder(path);
    } else {
        showActivity(info.absoluteFilePath());
        loadFolder(info.absolutePath());
    }
}

void MainWindow::loadFolder(const QString& folder) {
    if (scan_.isRunning()) return;
    listPage_->setBusy(tr("Reading activities from %1…").arg(QDir::toNativeSeparators(folder)));
    scan_.setFuture(QtConcurrent::run(scanFolder, folder));  // decode on a worker thread
}

void MainWindow::showActivity(const QString& path) {
    QString error;
    if (!detailPage_->showActivity(path, &error)) {
        QMessageBox::warning(this, tr("FIT Viewer"), tr("Failed to read %1:\n%2").arg(path, error));
        return;
    }
    pages_->setCurrentWidget(detailPage_);
}

void MainWindow::showList() { pages_->setCurrentWidget(listPage_); }

void MainWindow::openFolderDialog() {
    const QString start = watchPresent_ ? watchRoot() : currentFolder_;
    const QString path = QFileDialog::getExistingDirectory(this, tr("Open watch or folder"), start);
    if (!path.isEmpty()) {
        showList();
        loadFolder(path);
    }
}

void MainWindow::openFileDialog() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open FIT file"), currentFolder_,
                                                      tr("FIT files (*.fit *.FIT);;All files (*)"));
    if (!path.isEmpty()) openPath(path);
}

void MainWindow::sendToGarminConnect(const QString& path) {
    if (path.isEmpty()) return;
    const auto result = garmin_connect::sendFile(path);
    if (!result.browserOpened) {
        QMessageBox::warning(this, tr("Send to Garmin Connect"),
                             tr("Could not open a browser. Go to %1 manually.")
                                 .arg(QString::fromLatin1(garmin_connect::kImportUrl)));
        return;
    }
    const QString where = result.fileRevealed ? tr("The file is selected in Files: drag it onto the page.")
                                              : tr("Its folder is open in Files: drag the file onto the page.");
    statusBar()->showMessage(
        tr("Garmin Connect import page opened. %1 (Path copied: Ctrl+L, Ctrl+V in the browser's file dialog.)")
            .arg(where),
        15000);
}
