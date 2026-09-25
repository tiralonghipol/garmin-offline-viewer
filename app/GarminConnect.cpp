#include "GarminConnect.hpp"

#include <QClipboard>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>

namespace garmin_connect {
namespace {

// org.freedesktop.FileManager1.ShowItems opens the containing folder with the
// file selected. Implemented by Nautilus (Ubuntu's default), Dolphin, Nemo...
bool revealInFileManager(const QString& path) {
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    call << QStringList{QUrl::fromLocalFile(path).toString()} << QString{};
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 3000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

}  // namespace

SendResult sendFile(const QString& fitFilePath) {
    const QString absolute = QFileInfo(fitFilePath).absoluteFilePath();
    QGuiApplication::clipboard()->setText(absolute);

    SendResult result;
    result.fileRevealed = revealInFileManager(absolute);
    if (!result.fileRevealed) {
        // No FileManager1 service: at least open the folder.
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(absolute).absolutePath()));
    }
    result.browserOpened = QDesktopServices::openUrl(QUrl(QString::fromLatin1(kImportUrl)));
    return result;
}

}  // namespace garmin_connect
