# Sets FITVIEWER_VERSION_STRING from `git describe` (e.g. "0.2.0" on tag v0.2.0,
# "0.2.0-3-gabc1234" three commits later, "-dirty" with local changes), falling
# back to PROJECT_VERSION outside a git checkout. Evaluated at configure time:
# re-run cmake (CI always does) to refresh it.
set(FITVIEWER_VERSION_STRING "${PROJECT_VERSION}")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE git_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE git_result)
    if(git_result EQUAL 0 AND git_version MATCHES "^v?[0-9]")
        string(REGEX REPLACE "^v" "" FITVIEWER_VERSION_STRING "${git_version}")
    elseif(git_result EQUAL 0)  # no tags yet: just a commit hash
        set(FITVIEWER_VERSION_STRING "${PROJECT_VERSION}-${git_version}")
    endif()
endif()
message(STATUS "fitviewer version: ${FITVIEWER_VERSION_STRING}")
