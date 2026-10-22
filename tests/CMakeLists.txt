# Jimmy Paputto 2026

if(NOT TARGET gnsshat)
    message(FATAL_ERROR
        "tests/ must be built via the root CMakeLists.txt with -DBUILD_TESTS=ON.\n"
        "Run: cmake .. -DBUILD_TESTS=ON"
    )
endif()

find_package(GTest REQUIRED)

add_executable(GnssHatTests
    TestUtils.cpp
    TestUbxChecksum.cpp
    TestGnssConfig.cpp
    TestUbxMessages.cpp
    TestRtcm3.cpp
    TestUbxClassMsgId.cpp
    TestNtrip.cpp
    TestHatLifecycle.cpp
    TestCApi.cpp
    TestNmeaForwarder.cpp
)
set_target_properties(GnssHatTests PROPERTIES OUTPUT_NAME gnsshat-tests)

target_include_directories(GnssHatTests PRIVATE ${GNSSHAT_ROOT_DIR}/src)
target_link_libraries(GnssHatTests gnsshat GTest::GTest GTest::Main)

add_test(NAME GnssHatTests COMMAND GnssHatTests)

# Builds tests/subproject, which pulls gnsshat in with add_subdirectory and
# includes <jimmypaputto/...> the way a FetchContent consumer does.
add_test(NAME SubprojectIncludeLayout
    COMMAND ${CMAKE_CTEST_COMMAND}
        --build-and-test
            ${GNSSHAT_ROOT_DIR}/tests/subproject
            ${CMAKE_CURRENT_BINARY_DIR}/subproject
        --build-generator ${CMAKE_GENERATOR}
        --build-target consumer
        --build-options
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DGNSSHAT_PLATFORM=${GNSSHAT_PLATFORM}
)
