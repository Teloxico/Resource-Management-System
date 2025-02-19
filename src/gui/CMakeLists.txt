# File: src/gui/CMakeLists.txt

# Find the required Qt modules
find_package(Qt5 COMPONENTS Widgets REQUIRED)

# Create the GUI executable
add_executable(ResourceMonitorGUI main.cpp)

# Include directories
target_include_directories(ResourceMonitorGUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

# Link libraries
target_link_libraries(ResourceMonitorGUI PRIVATE
    ResourceCore
    Utils
    Qt5::Widgets
)

