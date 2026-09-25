#include <QApplication>

#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FIT Viewer"));

    MainWindow window;
    window.resize(1100, 700);
    window.show();

    // Optional: fitviewer <file.fit | watch folder>
    if (const auto args = QApplication::arguments(); args.size() > 1) {
        window.openPath(args.at(1));
    }
    return QApplication::exec();
}
