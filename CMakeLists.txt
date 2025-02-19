# File: CMakeLists.txt

# Specify the minimum required version of CMake
cmake_minimum_required(VERSION 3.15)

# Declare the project name
project(ResourceMonitor)

# Set the C++ standard to C++17 and make it required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Append custom CMake module path for finding additional packages
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# If compiling on a UNIX system
if(UNIX)
    # Attempt to find the PCAP library, which is required for network monitoring
    find_package(PCAP REQUIRED)
    # If PCAP is not found, display a fatal error message
    if(NOT PCAP_FOUND)
        message(FATAL_ERROR "libpcap not found. Please install libpcap-dev.")
    endif()
endif()

# Define an option to build the GUI; default is OFF
option(BUILD_GUI "Build the GUI application" OFF)

# Add the 'src' and 'src/tests' directories to the build
add_subdirectory(src)
add_subdirectory(src/tests)

# Display status messages about the project directories
message(STATUS "Project source dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Project binary dir: ${CMAKE_BINARY_DIR}")

# If on UNIX, display PCAP include and library directories
if(UNIX)
    message(STATUS "PCAP_INCLUDE_DIR: ${PCAP_INCLUDE_DIR}")
    message(STATUS "PCAP_LIBRARY: ${PCAP_LIBRARY}")
endif()

