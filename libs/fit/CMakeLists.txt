add_library(fit
    src/activity.cpp
    src/analysis.cpp
    src/crc.cpp
    src/decoder.cpp
    src/format.cpp
    src/geo.cpp
    src/types.cpp
)
# Namespaced alias: consumers write fitviewer::fit, and a typo becomes a
# configure-time error instead of a silent "-lfitt" link flag.
add_library(fitviewer::fit ALIAS fit)

# FILE_SET HEADERS also sets the include directory (PUBLIC -> propagated to
# anything that links fit) and would be picked up by install(TARGETS ...).
target_sources(fit
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/fit/activity.hpp
            include/fit/analysis.hpp
            include/fit/crc.hpp
            include/fit/decoder.hpp
            include/fit/format.hpp
            include/fit/geo.hpp
            include/fit/types.hpp
)

target_compile_features(fit PUBLIC cxx_std_20)
fitviewer_set_warnings(fit)
