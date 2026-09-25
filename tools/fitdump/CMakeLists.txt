add_executable(fitdump main.cpp)
target_link_libraries(fitdump PRIVATE fitviewer::fit)
fitviewer_set_warnings(fitdump)
