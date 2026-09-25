# All four modules come with qt6-base-dev on Ubuntu.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network Concurrent DBus)

# Enables AUTOMOC/AUTOUIC and sane defaults (Qt >= 6.3).
qt_standard_project_setup()

qt_add_executable(fitviewer
    main.cpp
    ActivityCardDelegate.cpp  ActivityCardDelegate.hpp
    ActivityDetailPage.cpp    ActivityDetailPage.hpp
    ActivityListModel.cpp     ActivityListModel.hpp
    ActivityListPage.cpp      ActivityListPage.hpp
    ChartWidget.cpp           ChartWidget.hpp
    GarminConnect.cpp         GarminConnect.hpp
    MainWindow.cpp            MainWindow.hpp
    Metrics.cpp               Metrics.hpp
    RouteMapWidget.cpp        RouteMapWidget.hpp
    Theme.cpp                 Theme.hpp
)
target_link_libraries(fitviewer PRIVATE
    fitviewer::fit
    Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::DBus
)
fitviewer_set_warnings(fitviewer)
target_compile_definitions(fitviewer PRIVATE FITVIEWER_VERSION="${FITVIEWER_VERSION_STRING}")

# Window icon, compiled into the binary as a Qt resource (":/fit-viewer.png").
qt_add_resources(fitviewer "icons"
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/packaging"
    FILES "${PROJECT_SOURCE_DIR}/packaging/fit-viewer.png"
)

# Standard Linux layout: used by `cmake --install` and to assemble the AppImage.
install(TARGETS fitviewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${PROJECT_SOURCE_DIR}/packaging/fit-viewer.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES "${PROJECT_SOURCE_DIR}/packaging/fit-viewer.png"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES "${PROJECT_SOURCE_DIR}/packaging/fit-viewer.svg"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
