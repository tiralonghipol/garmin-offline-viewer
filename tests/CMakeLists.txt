# With Conan this resolves to the gtest package via the generated
# GTestConfig.cmake; without Conan it falls back to a system install.
find_package(GTest REQUIRED)

add_executable(fit_tests
    fit_builder.hpp
    test_activity.cpp
    test_analysis.cpp
    test_crc.cpp
    test_decoder.cpp
    test_format.cpp
    test_geo.cpp
)
target_link_libraries(fit_tests PRIVATE fitviewer::fit GTest::gtest_main)
fitviewer_set_warnings(fit_tests)

# Registers every TEST() as its own ctest entry (queried from the binary at
# build time), so `ctest -R Decoder` or IDE test explorers work per test.
include(GoogleTest)
gtest_discover_tests(fit_tests)
