# File: src/utils/CMakeLists.txt

# Create the Utils library
add_library(Utils
    logger.cpp
)

# Include directories for the Utils library
target_include_directories(Utils PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(Utils PUBLIC ws2_32)
elseif(UNIX)
    # Add any Linux-specific libraries if necessary
endif()

