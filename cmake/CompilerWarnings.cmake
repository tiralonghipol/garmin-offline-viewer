# fitviewer_set_warnings(<target>)
# Applies a strict warning set to one target only (PRIVATE), so warnings are
# not pushed onto code that links against it.
function(fitviewer_set_warnings target)
    if(MSVC)
        set(warnings /W4 /permissive-)
        if(FITVIEWER_WARNINGS_AS_ERRORS)
            list(APPEND warnings /WX)
        endif()
    else()
        set(warnings -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
                     -Wold-style-cast -Wnon-virtual-dtor)
        if(FITVIEWER_WARNINGS_AS_ERRORS)
            list(APPEND warnings -Werror)
        endif()
    endif()
    target_compile_options(${target} PRIVATE ${warnings})
endfunction()
