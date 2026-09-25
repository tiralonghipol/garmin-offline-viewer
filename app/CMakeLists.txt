# qt6-base-dev on Ubuntu provides both Widgets and DBus.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets DBus)

# Enables AUTOMOC/AUTOUIC and sane defaults (Qt >= 6.3).
qt_standard_project_setup()

qt_add_executable(fitviewer
    main.cpp
    GarminConnect.cpp
    GarminConnect.hpp
    MainWindow.cpp
    MainWindow.hpp
    TrackPointModel.cpp
    TrackPointModel.hpp
)
target_link_libraries(fitviewer PRIVATE fitviewer::fit Qt6::Widgets Qt6::DBus)
fitviewer_set_warnings(fitviewer)
