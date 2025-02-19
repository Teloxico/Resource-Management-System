# File: src/tests/CMakeLists.txt

cmake_minimum_required(VERSION 3.15)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
if(WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)

FetchContent_MakeAvailable(googletest)

# Platform-specific libraries
if(WIN32)
    set(PLATFORM_LIBRARIES
        ws2_32
        iphlpapi
        psapi
        pdh
    )
elseif(UNIX)
    set(PLATFORM_LIBRARIES
        pthread
    )
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Collect test sources
set(TEST_SOURCES
    cpu_tests.cpp
    memory_tests.cpp
    network_tests.cpp
    database_tests.cpp
    logger_tests.cpp
)

# Add the test executable
add_executable(ResourceMonitorTests ${TEST_SOURCES})

# Include directories for tests
target_include_directories(ResourceMonitorTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries to the test executable
target_link_libraries(ResourceMonitorTests PRIVATE
    ResourceCore
    Utils
    gtest_main
    ${PLATFORM_LIBRARIES}  # Link platform-specific libraries
)

# Enable GoogleTest
include(GoogleTest)
gtest_discover_tests(ResourceMonitorTests)

