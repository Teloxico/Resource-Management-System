# src/CMakeLists.txt
add_subdirectory(utils)
add_subdirectory(core)

if(BUILD_CLI)
    add_subdirectory(cli)
endif()

if(BUILD_GUI)
    add_subdirectory(gui)
endif()
