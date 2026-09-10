cmake_minimum_required(VERSION 3.12)

project(PrintNavigationUsb)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(PrintNavigationUsb src/PrintNavigationUsb.cpp)
set_target_properties(PrintNavigationUsb PROPERTIES OUTPUT_NAME print-navigation-usb)
target_link_libraries(PrintNavigationUsb gnsshat)
