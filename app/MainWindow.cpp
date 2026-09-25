#include "MainWindow.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>
#include "GarminConnect.hpp"
#include "TrackPointModel.hpp"
#include "fit/activity.hpp"
#include "fit/format.hpp"

namespace {

QString summaryText(const fit::Activity& activity, const QString& fileName) {
    QStringList parts;
    if (activity.sessions.empty()) {
        parts << QObject::tr("no session summary");
    } else {
        const auto& s = activity.sessions.front();
        parts << QStringLiteral("<b>%1</b>")
                     .arg(QString::fromStdString(s.sport ? fit::sportName(*s.sport) : "Activity"));
        if (s.startTime) {
            parts << QDateTime::fromSecsSinceEpoch(s.startTime->time_since_epoch().count())
                         .toLocalTime()
                         .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        }
        if (s.totalDistanceM) parts << QStringLiteral("%1 km").arg(*s.totalDistanceM / 1000.0, 0, 'f', 2);
        if (s.totalTimerS) parts << QString::fromStdString(fit::formatDuration(*s.totalTimerS));
        if (s.totalDistanceM && s.totalTimerS && *s.totalTimerS > 0) {
            parts << QString::fromStdString(fit::formatPace(*s.totalDistanceM / *s.totalTimerS));
        }
        if (s.avgHeartRate) {
            parts << QObject::tr("HR avg %1 / max %2")
                         .arg(*s.avgHeartRate)
                         .arg(s.maxHeartRate ? QString::number(*s.maxHeartRate) : QStringLiteral("?"));
        }
        if (s.totalCalories) parts << QObject::tr("%1 kcal").arg(*s.totalCalories);
    }
    parts << QObject::tr("%n point(s)", nullptr, static_cast<int>(activity.points.size()));
    return QStringLiteral("%1<br><small>%2</small>").arg(parts.join(QStringLiteral(" · ")), fileName);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("FIT Viewer"));

    fileList_ = new QListWidget;
    summary_ = new QLabel(tr("Open a .fit file or the watch folder (File menu)."));
    summary_->setTextFormat(Qt::RichText);
    summary_->setMargin(8);

    model_ = new TrackPointModel(this);
    table_ = new QTableView;
    table_->setModel(model_);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(table_->fontMetrics().height() + 6);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto* right = new QWidget;
    auto* layout = new QVBoxLayout(right);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(summary_);
    layout->addWidget(table_, 1);

    auto* splitter = new QSplitter;
    splitter->addWidget(fileList_);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* openFile = fileMenu->addAction(tr("Open &file…"), QKeySequence::Open, this,
                                         &MainWindow::openFileDialog);
    auto* openFolder = fileMenu->addAction(tr("Open &watch folder…"), QKeySequence(tr("Ctrl+Shift+O")),
                                           this, &MainWindow::openFolderDialog);
    fileMenu->addSeparator();
    sendAction_ = fileMenu->addAction(tr("&Send to Garmin Connect…"), QKeySequence(tr("Ctrl+U")),
                                      this, &MainWindow::sendToGarminConnect);
    sendAction_->setToolTip(tr("Open Garmin Connect's import page with this file ready to drop"));
    sendAction_->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->addAction(openFile);
    toolbar->addAction(openFolder);
    toolbar->addSeparator();
    toolbar->addAction(sendAction_);

    // Right-click on a file in the list offers the same action.
    fileList_->setContextMenuPolicy(Qt::ActionsContextMenu);
    fileList_->addAction(sendAction_);

    connect(fileList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item) loadFile(item->data(Qt::UserRole).toString());
    });

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::openPath(const QString& path) {
    QFileInfo(path).isDir() ? loadFolder(path) : loadFile(path);
}

void MainWindow::openFileDialog() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open FIT file"), {},
                                                      tr("FIT files (*.fit *.FIT);;All files (*)"));
    if (!path.isEmpty()) loadFile(path);
}

void MainWindow::openFolderDialog() {
    const QString path = QFileDialog::getExistingDirectory(this, tr("Open watch folder"));
    if (!path.isEmpty()) loadFolder(path);
}

void MainWindow::loadFolder(const QString& path) {
    // Accept the watch root, its GARMIN folder, or the ACTIVITY folder itself.
    QDir dir(path);
    for (const auto& sub : {QStringLiteral("GARMIN/ACTIVITY"), QStringLiteral("ACTIVITY")}) {
        if (dir.exists(sub)) {
            dir.cd(sub);
            break;
        }
    }

    fileList_->clear();
    const auto entries = dir.entryInfoList({QStringLiteral("*.fit")}, QDir::Files, QDir::Time);
    for (const QFileInfo& info : entries) {
        auto* item = new QListWidgetItem(info.fileName(), fileList_);
        item->setData(Qt::UserRole, info.absoluteFilePath());
        item->setToolTip(info.lastModified().toString());
    }
    statusBar()->showMessage(tr("%n activity file(s) in %1", nullptr, static_cast<int>(entries.size()))
                                 .arg(QDir::toNativeSeparators(dir.absolutePath())));
    if (!entries.isEmpty()) fileList_->setCurrentRow(0);
}

void MainWindow::loadFile(const QString& path) {
    try {
        auto activity = fit::loadActivity(path.toStdString());  // UTF-8 on Linux
        summary_->setText(summaryText(activity, QDir::toNativeSeparators(path)));
        model_->setPoints(std::move(activity.points));
        currentFile_ = path;
        sendAction_->setEnabled(true);
        statusBar()->showMessage(tr("Loaded %1").arg(QFileInfo(path).fileName()), 5000);
    } catch (const std::exception& e) {
        model_->setPoints({});
        currentFile_.clear();
        sendAction_->setEnabled(false);
        summary_->setText(tr("Could not read file."));
        QMessageBox::warning(this, tr("FIT Viewer"),
                             tr("Failed to read %1:\n%2").arg(path, QString::fromUtf8(e.what())));
    }
}

void MainWindow::sendToGarminConnect() {
    if (currentFile_.isEmpty()) return;
    const auto result = garmin_connect::sendFile(currentFile_);

    QString where = result.fileRevealed
                        ? tr("The file is selected in your file manager: drag it onto the page.")
                        : tr("Its folder is open in your file manager: drag the file onto the page.");
    if (!result.browserOpened) {
        QMessageBox::warning(this, tr("Send to Garmin Connect"),
                             tr("Could not open a browser. Go to %1 manually.")
                                 .arg(QString::fromLatin1(garmin_connect::kImportUrl)));
        return;
    }
    statusBar()->showMessage(
        tr("Garmin Connect import page opened. %1 Path copied: use Ctrl+L, Ctrl+V in the browser's "
           "file dialog.").arg(where),
        15000);
}
