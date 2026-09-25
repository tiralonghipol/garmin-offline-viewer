# `appimage` target: build/<type>/fit-viewer-<version>-x86_64.AppImage
#
#   1. cmake --install into an AppDir (usr/bin, desktop file, icons)
#   2. linuxdeploy + its Qt plugin copy Qt libraries and plugins into the AppDir
#   3. appimagetool packs the AppDir with the static type2 runtime, so users
#      don't need libfuse2 (not installed by default on Ubuntu 24.04+)
#
# The tools run with APPIMAGE_EXTRACT_AND_RUN=1, so building needs no FUSE
# either (containers, CI).

option(FITVIEWER_APPIMAGE_ON_BUILD "Package the AppImage as part of every build (adds ~30-60 s)" OFF)

set(appimage_tools "${CMAKE_BINARY_DIR}/appimage-tools")
set(appdir "${CMAKE_BINARY_DIR}/AppDir")
set(FITVIEWER_APPIMAGE "${CMAKE_BINARY_DIR}/fit-viewer-${FITVIEWER_VERSION_STRING}-x86_64.AppImage")

if(FITVIEWER_APPIMAGE_ON_BUILD)
    set(in_all ALL)
endif()

add_custom_target(appimage ${in_all}
    COMMAND "${CMAKE_COMMAND}" -DTOOLS_DIR=${appimage_tools} -P "${CMAKE_CURRENT_LIST_DIR}/FetchAppImageTools.cmake"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${appdir}"
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${appdir}/usr"
    COMMAND "${CMAKE_COMMAND}" -E env APPIMAGE_EXTRACT_AND_RUN=1 QMAKE=$<TARGET_FILE:Qt6::qmake>
            "${appimage_tools}/linuxdeploy-x86_64.AppImage"
            --appdir "${appdir}"
            --executable "${appdir}/usr/bin/fitviewer"
            --desktop-file "${appdir}/usr/share/applications/fit-viewer.desktop"
            --icon-file "${appdir}/usr/share/icons/hicolor/256x256/apps/fit-viewer.png"
            --plugin qt
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${FITVIEWER_APPIMAGE}"
    COMMAND "${CMAKE_COMMAND}" -E env APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64
            "${appimage_tools}/appimagetool-x86_64.AppImage" --no-appstream "${appdir}" "${FITVIEWER_APPIMAGE}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Packaging ${FITVIEWER_APPIMAGE}"
    VERBATIM
    USES_TERMINAL
)
add_dependencies(appimage fitviewer)
if(TARGET fitdump)
    add_dependencies(appimage fitdump)  # installed alongside, so it must be built
endif()
