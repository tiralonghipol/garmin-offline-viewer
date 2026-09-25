#include <QApplication>

#include "MainWindow.hpp"
#include "Theme.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // Used by QSettings (~/.config/fit-viewer) and the tile cache (~/.cache/fit-viewer).
    QApplication::setOrganizationName(QStringLiteral("fit-viewer"));
    QApplication::setApplicationName(QStringLiteral("fit-viewer"));
    QApplication::setApplicationDisplayName(QStringLiteral("FIT Viewer"));
    app.setStyleSheet(theme::styleSheet());

    MainWindow window;
    window.resize(1440, 900);
    window.show();

    // Optional: fitviewer <file.fit | folder | watch root>
    if (const auto args = QApplication::arguments(); args.size() > 1) {
        window.openPath(args.at(1));
    }
    return QApplication::exec();
}
