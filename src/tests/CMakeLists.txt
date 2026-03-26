# src/tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
if(WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
FetchContent_MakeAvailable(googletest)

set(TEST_SOURCES
    cpu_tests.cpp
    memory_tests.cpp
    network_tests.cpp
    disk_tests.cpp
    process_tests.cpp
    database_tests.cpp
    logger_tests.cpp
    alert_tests.cpp
)

add_executable(ResourceMonitorTests ${TEST_SOURCES})

target_include_directories(ResourceMonitorTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ResourceMonitorTests PRIVATE
    ResourceCore
    Utils
    gtest_main
)

include(GoogleTest)
gtest_discover_tests(ResourceMonitorTests)
