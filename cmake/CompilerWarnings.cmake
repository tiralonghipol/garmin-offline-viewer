# fitviewer_set_warnings(<target>)
# Applies a strict GCC/Clang warning set to one target only (PRIVATE), so
# warnings are not pushed onto code that links against it.
function(fitviewer_set_warnings target)
    set(warnings -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
                 -Wold-style-cast -Wnon-virtual-dtor)
    if(FITVIEWER_WARNINGS_AS_ERRORS)
        list(APPEND warnings -Werror)
    endif()
    target_compile_options(${target} PRIVATE ${warnings})
endfunction()
