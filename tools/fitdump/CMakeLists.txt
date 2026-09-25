add_executable(fitdump main.cpp)
target_link_libraries(fitdump PRIVATE fitviewer::fit)
fitviewer_set_warnings(fitdump)
install(TARGETS fitdump RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
