# File: src/CMakeLists.txt

# Add subdirectories for building
add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(cli)

# Optionally build GUI if enabled
if(BUILD_GUI)
    add_subdirectory(gui)
endif()

