# src/utils/CMakeLists.txt
add_library(Utils
    logger.cpp
    logger.h
    scrolling_buffer.h
)

target_include_directories(Utils PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
