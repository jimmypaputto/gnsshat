# Jimmy Paputto 2025

cmake_minimum_required(VERSION 3.12)

project(gnsshat VERSION 1.0.0)

# Absolute paths to this project, so they stay correct when gnsshat is pulled
# into another build with add_subdirectory or FetchContent -- CMAKE_SOURCE_DIR
# points at the outer project in that case. Subdirectories inherit these, and
# some of them call project() themselves, which rules out PROJECT_SOURCE_DIR.
set(GNSSHAT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(GNSSHAT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Installing headers, exporting a CMake package, writing a pkg-config file and
# running ldconfig are things only a standalone build should do.
if(CMAKE_SOURCE_DIR STREQUAL GNSSHAT_ROOT_DIR)
    set(GNSSHAT_TOP_LEVEL TRUE)
else()
    set(GNSSHAT_TOP_LEVEL FALSE)
    message(STATUS "gnsshat built as a subproject - install rules disabled")
endif()

option(BUILD_PYTHON "Build Python bindings" OFF)
option(BUILD_EXAMPLES "Build C and C++ examples" OFF)
option(BUILD_TOOLS "Build CLI tools (gnsshat-info, gnsshat-probe)" OFF)
option(BUILD_TESTS "Build unit tests (requires GTest)" OFF)
option(NTRIP_TLS_SUPPORT "Enable TLS for NTRIP connections (requires OpenSSL)" OFF)

set(GNSSHAT_PLATFORM "auto" CACHE STRING "Target platform: auto, generic, rpi4, or rpi5")
set_property(CACHE GNSSHAT_PLATFORM PROPERTY STRINGS auto generic rpi4 rpi5)

include(CheckSymbolExists)

set(_GNSSHAT_CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
list(APPEND CMAKE_REQUIRED_LIBRARIES gpiod)
check_symbol_exists(gpiod_line_settings_new "gpiod.h" GNSSHAT_HAS_LIBGPIOD_V2)
set(CMAKE_REQUIRED_LIBRARIES "${_GNSSHAT_CMAKE_REQUIRED_LIBRARIES}")
unset(_GNSSHAT_CMAKE_REQUIRED_LIBRARIES)

if(GNSSHAT_HAS_LIBGPIOD_V2)
    set(LIBGPIOD_VERSION 2)
else()
    set(LIBGPIOD_VERSION 1)
endif()

add_compile_definitions(LIBGPIOD_VERSION=${LIBGPIOD_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include(GNUInstallDirs)

set(GNSSHAT_DETECTED_PLATFORM "${GNSSHAT_PLATFORM}")

if(GNSSHAT_DETECTED_PLATFORM STREQUAL "auto" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    execute_process(
        COMMAND grep "Model" /proc/cpuinfo
        OUTPUT_VARIABLE RPI_MODEL
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(RPI_MODEL MATCHES "Raspberry Pi 5")
        set(GNSSHAT_DETECTED_PLATFORM "rpi5")
    elseif(RPI_MODEL MATCHES "Raspberry Pi 4")
        set(GNSSHAT_DETECTED_PLATFORM "rpi4")
    endif()
endif()

if(GNSSHAT_DETECTED_PLATFORM STREQUAL "rpi5")
    add_definitions(-DRPI5)
    if(LIBGPIOD_VERSION MATCHES 2)
        add_compile_definitions(CHIP_NAME="/dev/gpiochip4")
        message(STATUS "Using libgpiod v2 API")
    else()
        add_compile_definitions(CHIP_NAME="gpiochip4")
        message(STATUS "Using libgpiod v1 API")
    endif()
    message(STATUS "Building for Raspberry Pi 5")
elseif(GNSSHAT_DETECTED_PLATFORM STREQUAL "rpi4")
    if(LIBGPIOD_VERSION MATCHES 2)
        add_compile_definitions(CHIP_NAME="/dev/gpiochip0")
        message(STATUS "Using libgpiod v2 API")
    else()
        add_compile_definitions(CHIP_NAME="gpiochip0")
        message(STATUS "Using libgpiod v1 API")
    endif()
    add_definitions(-DRPI4)
    message(STATUS "Building for Raspberry Pi 4")
elseif(GNSSHAT_DETECTED_PLATFORM STREQUAL "generic")
    if(LIBGPIOD_VERSION MATCHES 2)
        add_compile_definitions(CHIP_NAME="/dev/gpiochip0")
        message(STATUS "Using libgpiod v2 API")
    else()
        add_compile_definitions(CHIP_NAME="gpiochip0")
        message(STATUS "Using libgpiod v1 API")
    endif()
    add_compile_definitions(GNSSHAT_GENERIC_LINUX=1)
    message(WARNING "Building in generic Linux mode; Raspberry Pi hardware-specific behavior is not validated in this configuration.")
elseif(GNSSHAT_DETECTED_PLATFORM STREQUAL "auto")
    message(
        FATAL_ERROR
        "Unsupported platform: ${CMAKE_SYSTEM_PROCESSOR}. Raspberry Pi 4 or 5 is required."
    )
else()
    message(FATAL_ERROR "Unsupported GNSSHAT_PLATFORM value: ${GNSSHAT_PLATFORM}")
endif()

configure_file(
    ${GNSSHAT_ROOT_DIR}/src/Version.hpp.in
    ${GNSSHAT_ROOT_DIR}/src/Version.hpp
    @ONLY
)

include_directories(
    src
)

set(LIB_SOURCES
    src/ntrip/NtripCaster.cpp
    src/ntrip/NtripClient.cpp
    src/ntrip/NtripServer.cpp
    src/common/BuildInfo.cpp
    src/common/GpioInterruptLine.cpp
    src/common/JPGuard.cpp
    src/common/Utils.cpp
    src/ublox/ubxmsg/IUbxMsg.cpp
    src/ublox/ubxmsg/UBX_CFG_VALGET.cpp
    src/ublox/BaseConfig.cpp
    src/ublox/GnssConfig.cpp
    src/ublox/NavigationStore.cpp
    src/ublox/NmeaForwarder.cpp
    src/ublox/Rtcm3Parser.cpp
    src/ublox/Rtcm3Store.cpp
    src/ublox/RTK.cpp
    src/ublox/Run.cpp
    src/ublox/SpiDriver.cpp
    src/ublox/Startup.cpp
    src/ublox/UartDriver.cpp
    src/ublox/Ublox.cpp
    src/ublox/UbloxConfigRegistry.cpp
    src/ublox/UbxCallbacks.cpp
    src/ublox/UbxFactory.cpp
    src/ublox/UbxParser.cpp
    src/GnssHat.cpp
    src/GnssHat_C.cpp
)

add_library(gnsshat SHARED ${LIB_SOURCES})

# Installed headers live in <includedir>/jimmypaputto, and public sources include
# them as <jimmypaputto/...>, so the exported include dir must be <includedir>.
target_include_directories(gnsshat
    PUBLIC
        $<BUILD_INTERFACE:${GNSSHAT_ROOT_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Git info for BuildInfo
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${GNSSHAT_ROOT_DIR}
    OUTPUT_VARIABLE GIT_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY ${GNSSHAT_ROOT_DIR}
    OUTPUT_VARIABLE GIT_BUILD_NUMBER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_REVISION)
    set(GIT_REVISION "unknown")
endif()
if(NOT GIT_BUILD_NUMBER)
    set(GIT_BUILD_NUMBER 0)
endif()
set_source_files_properties(src/common/BuildInfo.cpp PROPERTIES
    COMPILE_DEFINITIONS "GNSSHAT_GIT_REVISION=\"${GIT_REVISION}\";GNSSHAT_BUILD_NUMBER=${GIT_BUILD_NUMBER}"
)

# Force BuildInfo.cpp recompile on every build for fresh __DATE__/__TIME__
add_custom_target(BuildInfoRefresh ALL
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${GNSSHAT_ROOT_DIR}/src/common/BuildInfo.cpp
)
add_dependencies(gnsshat BuildInfoRefresh)

set_target_properties(gnsshat PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_link_libraries(gnsshat gpiod pthread util)

if(NTRIP_TLS_SUPPORT)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(gnsshat PUBLIC GNSSHAT_HAS_TLS)
    target_link_libraries(gnsshat OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "NTRIP TLS support enabled")
endif()

if(BUILD_TOOLS)
    find_package(toml11 REQUIRED)
    add_subdirectory(tools)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    file(MAKE_DIRECTORY ${GNSSHAT_BUILD_DIR}/include)
    execute_process(COMMAND ln -snf ${GNSSHAT_ROOT_DIR}/src ${GNSSHAT_BUILD_DIR}/include/jimmypaputto)
    target_include_directories(gnsshat PUBLIC $<BUILD_INTERFACE:${GNSSHAT_BUILD_DIR}/include>)

    # C examples
    add_subdirectory(examples/c/geofencing)
    add_subdirectory(examples/c/hot-start)
    add_subdirectory(examples/c/jamming-detector)
    add_subdirectory(examples/c/print-navigation)
    add_subdirectory(examples/c/print-satellites)
    add_subdirectory(examples/c/rtk)
    add_subdirectory(examples/c/time-base)
    add_subdirectory(examples/c/time-mark)
    add_subdirectory(examples/c/timepulse-interrupt)

    # C++ examples
    add_subdirectory(examples/cpp/geofencing)
    add_subdirectory(examples/cpp/hot-start)
    add_subdirectory(examples/cpp/jamming-detector)
    add_subdirectory(examples/cpp/print-navigation)
    add_subdirectory(examples/cpp/print-satellites)
    add_subdirectory(examples/cpp/rtk)
    add_subdirectory(examples/cpp/time-base)
    add_subdirectory(examples/cpp/time-mark)
    add_subdirectory(examples/cpp/timepulse-interrupt)

    # Create bin directory with symlinks to C++ example binaries
    set(SYMLINK_DIR ${GNSSHAT_ROOT_DIR}/examples/bin)
    file(MAKE_DIRECTORY ${SYMLINK_DIR})

    set(CPP_TARGETS
        Geofencing
        HotStart
        JammingDetector
        PrintNavigationAdvanced
        PrintNavigationBasic
        PrintSatellitesBasic
        PrintSatellitesAdvanced
        RtkBase
        RtkRover
        TimeBase
        TimeMark
        TimepulseInterrupt
    )

    # Map CMake target names to output binary names for symlinks
    set(CPP_SYMLINK_NAMES
        geofencing
        hot-start
        jamming-detector
        print-navigation-advanced
        print-navigation-basic
        print-satellites-basic
        print-satellites-advanced
        rtk-base
        rtk-rover
        time-base
        time-mark
        timepulse-interrupt
    )

    set(SYMLINK_COMMANDS)
    list(LENGTH CPP_TARGETS _len)
    math(EXPR _last "${_len} - 1")
    foreach(_i RANGE ${_last})
        list(GET CPP_TARGETS ${_i} _target)
        list(GET CPP_SYMLINK_NAMES ${_i} _name)
        list(APPEND SYMLINK_COMMANDS
            COMMAND ln -snf $<TARGET_FILE:${_target}> ${SYMLINK_DIR}/${_name}
        )
    endforeach()

    add_custom_target(BinariesSymlinks ALL
        ${SYMLINK_COMMANDS}
        DEPENDS ${CPP_TARGETS}
        COMMENT "Creating symlinks in examples/bin/"
    )

    set(SYMLINK_FILES)
    foreach(_name ${CPP_SYMLINK_NAMES})
        list(APPEND SYMLINK_FILES ${SYMLINK_DIR}/${_name})
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${SYMLINK_FILES})
endif()

# ── Installation ─────────────────────────────────────────────────────────────
#
# Everything below only makes sense for a standalone build. When gnsshat is
# embedded in another project, installing its headers and CMake package into
# that project's prefix, rewriting the pkg-config file, running ldconfig and
# declaring an `uninstall` target are all wrong -- and the last one collides
# outright with the `uninstall` target ament_cmake already defines.
if(NOT GNSSHAT_TOP_LEVEL)
    return()
endif()

install(TARGETS gnsshat
    EXPORT gnsshatTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    FILES
        src/ublox/DilutionOverPrecision.hpp
        src/ublox/EDynamicModel.hpp
        src/ublox/EFixQuality.hpp
        src/ublox/EFixStatus.hpp
        src/ublox/EFixType.hpp
        src/ublox/ERtkMode.hpp
        src/ublox/Geofence.hpp
        src/ublox/Geofencing.hpp
        src/ublox/GnssConfig.hpp
        src/ublox/Navigation.hpp
        src/ublox/PositionVelocityTime.hpp
        src/ublox/RFBlock.hpp
        src/ublox/RTK.hpp
        src/ublox/BaseConfig.hpp
        src/ublox/RtkConfig.hpp
        src/ublox/TimingConfig.hpp
        src/ublox/SatelliteInfo.hpp
        src/ublox/SystemHealth.hpp
        src/ublox/TimeMark.hpp
        src/ublox/TimepulsePinConfig.hpp
        src/ublox/RFBlockSpectrumData.hpp
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/jimmypaputto/ublox
)

install(
    FILES
        src/GnssHat.hpp
        src/GnssHat.h
        src/Version.hpp
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/jimmypaputto
)

install(
    FILES
        src/common/BuildInfo.hpp
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/jimmypaputto/common
)

install(
    FILES
        src/ntrip/NtripCaster.hpp
        src/ntrip/NtripClient.hpp
        src/ntrip/NtripServer.hpp
        src/ntrip/NtripLog.hpp
        src/ntrip/NtripStats.hpp
        src/ntrip/NtripTls.hpp
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/jimmypaputto/ntrip
)

# ── CMake package config (enables find_package(gnsshat)) ────────────────────

include(CMakePackageConfigHelpers)

install(EXPORT gnsshatTargets
    FILE gnsshatTargets.cmake
    NAMESPACE gnsshat::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gnsshat
)

configure_package_config_file(
    ${GNSSHAT_ROOT_DIR}/cmake/gnsshatConfig.cmake.in
    ${GNSSHAT_BUILD_DIR}/gnsshatConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gnsshat
)

write_basic_package_version_file(
    ${GNSSHAT_BUILD_DIR}/gnsshatConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${GNSSHAT_BUILD_DIR}/gnsshatConfig.cmake
        ${GNSSHAT_BUILD_DIR}/gnsshatConfigVersion.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/gnsshat
)

# ── pkg-config ───────────────────────────────────────────────────────────────

configure_file(
    ${GNSSHAT_ROOT_DIR}/cmake/gnsshat.pc.in
    ${GNSSHAT_BUILD_DIR}/gnsshat.pc
    @ONLY
)

install(
    FILES ${GNSSHAT_BUILD_DIR}/gnsshat.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

install(CODE "
    if(EXISTS \"/etc/ld.so.conf\")
        execute_process(COMMAND ldconfig)
    endif()
")

file(WRITE ${GNSSHAT_BUILD_DIR}/cmake_uninstall.cmake "
    set(MANIFEST \"\${CMAKE_BINARY_DIR}/install_manifest.txt\")
    if(NOT EXISTS \${MANIFEST})
        message(FATAL_ERROR \"Cannot find install manifest: \${MANIFEST}\")
    endif()
    file(READ \${MANIFEST} FILES)
    string(STRIP \"\${FILES}\" FILES)
    string(REGEX REPLACE \"\\n\" \";\" FILES \"\${FILES}\")
    set(FAILED_FILES)
    foreach(FILE \${FILES})
        if(IS_SYMLINK \"\${FILE}\" OR EXISTS \"\${FILE}\")
            message(STATUS \"Removing: \${FILE}\")
            file(REMOVE \"\${FILE}\")
            if(EXISTS \"\${FILE}\")
                list(APPEND FAILED_FILES \"\${FILE}\")
                message(WARNING \"Failed to remove: \${FILE} (permission denied?)\")
            else()
                get_filename_component(DIR \"\${FILE}\" DIRECTORY)
                if(IS_DIRECTORY \"\${DIR}\")
                    file(GLOB DIR_CONTENTS \"\${DIR}/*\")
                    if(NOT DIR_CONTENTS)
                        message(STATUS \"Removing empty directory: \${DIR}\")
                        file(REMOVE_RECURSE \"\${DIR}\")
                    endif()
                endif()
            endif()
        else()
            message(STATUS \"Not found (skipping): \${FILE}\")
        endif()
    endforeach()
    if(IS_DIRECTORY \"${CMAKE_INSTALL_FULL_INCLUDEDIR}/jimmypaputto\")
        message(STATUS \"Removing: ${CMAKE_INSTALL_FULL_INCLUDEDIR}/jimmypaputto\")
        file(REMOVE_RECURSE \"${CMAKE_INSTALL_FULL_INCLUDEDIR}/jimmypaputto\")
    endif()
    execute_process(COMMAND ldconfig)
    if(FAILED_FILES)
        string(REPLACE \";\" \"\\n  \" FAILED_LIST \"\${FAILED_FILES}\")
        message(FATAL_ERROR \"Failed to remove the following files (try running with sudo):\\n  \${FAILED_LIST}\")
    endif()
    message(STATUS \"gnsshat uninstalled.\")
")

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BINARY_DIR=${GNSSHAT_BUILD_DIR} -P ${GNSSHAT_BUILD_DIR}/cmake_uninstall.cmake
    COMMENT "Uninstalling gnsshat"
)

if(BUILD_PYTHON)
    add_subdirectory(python)
endif()
