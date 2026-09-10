add_executable(gnsshat-info gnsshat-info.cpp)
target_include_directories(gnsshat-info PRIVATE ${GNSSHAT_ROOT_DIR}/src)
target_link_libraries(gnsshat-info gnsshat)

# gnsshat-probe: links against the shared library
add_executable(gnsshat-probe gnsshat-probe.cpp)
target_include_directories(gnsshat-probe PRIVATE ${GNSSHAT_ROOT_DIR}/src)
target_link_libraries(gnsshat-probe gnsshat)

# gnsshat-rtk-base: RTK base station tool with TOML config
add_executable(gnsshat-rtk-base gnsshat-rtk-base.cpp)
target_include_directories(gnsshat-rtk-base PRIVATE ${GNSSHAT_ROOT_DIR}/src)
target_link_libraries(gnsshat-rtk-base gnsshat toml11::toml11)

install(TARGETS gnsshat-info gnsshat-probe gnsshat-rtk-base
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
