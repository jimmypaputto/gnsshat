# Jimmy Paputto 2026

# Consumer that pulls gnsshat in with add_subdirectory, the way FetchContent
# does. Built by the SubprojectIncludeLayout test from tests/CMakeLists.txt.

cmake_minimum_required(VERSION 3.12)

project(gnsshat_subproject_consumer C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(GNSSHAT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
add_subdirectory(${GNSSHAT_SOURCE_DIR} gnsshat EXCLUDE_FROM_ALL)

add_executable(consumer consumer.cpp consumer_c.c)
target_link_libraries(consumer PRIVATE gnsshat)

# Compile options land after the include directories on the command line, so
# this -I is searched after every directory gnsshat exports but before the
# system ones. If gnsshat stops providing <jimmypaputto/...>, the lookup hits
# the #error headers here instead of silently using an installed copy.
target_compile_options(consumer PRIVATE -I${CMAKE_CURRENT_SOURCE_DIR}/include-guard)
