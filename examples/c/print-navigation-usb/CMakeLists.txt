cmake_minimum_required(VERSION 3.12)

project(PrintNavigationUsb_C)

add_executable(print_navigation_usb src/print_navigation_usb.c)
set_target_properties(print_navigation_usb PROPERTIES OUTPUT_NAME print-navigation-usb-c)
target_link_libraries(print_navigation_usb gnsshat pthread)
