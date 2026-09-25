#pragma once

#include <QString>

class QWidget;

// Garmin offers no public upload API for personal apps, so "upload" means:
// open Connect's web import page, highlight the file in the file manager and
// put its path on the clipboard. The user drags the file onto the page (or
// pastes the path into the browser's file dialog with Ctrl+L).
namespace garmin_connect {

inline constexpr auto kImportUrl = "https://connect.garmin.com/modern/import-data";

struct SendResult {
    bool browserOpened = false;
    bool fileRevealed = false;  // highlighted in Nautilus/Dolphin/... via D-Bus
};

SendResult sendFile(const QString& fitFilePath);

}  // namespace garmin_connect
