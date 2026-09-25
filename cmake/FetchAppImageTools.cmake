# cmake -DTOOLS_DIR=<dir> -P FetchAppImageTools.cmake
# Downloads the packaging tools once (they are AppImages themselves).
set(tools
    "linuxdeploy-x86_64.AppImage|https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    "linuxdeploy-plugin-qt-x86_64.AppImage|https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    "appimagetool-x86_64.AppImage|https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
)
file(MAKE_DIRECTORY "${TOOLS_DIR}")
foreach(entry IN LISTS tools)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 url)
    set(target "${TOOLS_DIR}/${name}")
    if(NOT EXISTS "${target}")
        message(STATUS "Downloading ${name}")
        file(DOWNLOAD "${url}" "${target}.part" STATUS status SHOW_PROGRESS)
        list(GET status 0 code)
        if(NOT code EQUAL 0)
            file(REMOVE "${target}.part")
            message(FATAL_ERROR "Failed to download ${url}: ${status}")
        endif()
        file(RENAME "${target}.part" "${target}")
        file(CHMOD "${target}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    endif()
endforeach()
