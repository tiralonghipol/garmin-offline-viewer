find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# Enables AUTOMOC/AUTOUIC and sane defaults (Qt >= 6.3).
qt_standard_project_setup()

qt_add_executable(fitviewer
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
    TrackPointModel.cpp
    TrackPointModel.hpp
)
target_link_libraries(fitviewer PRIVATE fitviewer::fit Qt6::Widgets)
fitviewer_set_warnings(fitviewer)

set_target_properties(fitviewer PROPERTIES
    WIN32_EXECUTABLE ON   # no console window on Windows
    MACOSX_BUNDLE ON
)
