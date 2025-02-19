# File: src/cli/CMakeLists.txt

# Add the executable for the CLI application
add_executable(ResourceMonitorCLI
    main.cpp
)

# Include directories for the CLI application
target_include_directories(ResourceMonitorCLI PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Link the core library and utilities to the CLI application
target_link_libraries(ResourceMonitorCLI PUBLIC
    ResourceCore
    Utils
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(ResourceMonitorCLI PUBLIC ws2_32)
elseif(UNIX)
    # Add any Linux-specific libraries if necessary
endif()

