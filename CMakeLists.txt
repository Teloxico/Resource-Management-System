# ===========================================================================
#  ResourceMonitor — root CMakeLists.txt
# ===========================================================================
cmake_minimum_required(VERSION 3.15)
project(ResourceMonitor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Custom module path (e.g. FindPCAP — kept for reference but no longer used)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Build options
option(BUILD_GUI   "Build the ImGui GUI application"  ON)
option(BUILD_CLI   "Build the CLI application"         ON)
option(BUILD_TESTS "Build the test suite"              ON)

# ===========================================================================
#  Third-party dependencies via FetchContent
# ===========================================================================
include(FetchContent)

# ---- GLFW (windowing) -----------------------------------------------------
if(BUILD_GUI)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.10
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glfw)

    # ---- Dear ImGui --------------------------------------------------------
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.90.9
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(imgui)

    # Build ImGui as a static library (core + GLFW + OpenGL3 backends)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw)
    if(WIN32)
        target_link_libraries(imgui PUBLIC opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(imgui PUBLIC OpenGL::GL ${CMAKE_DL_LIBS})
    endif()

    # ---- ImPlot ------------------------------------------------------------
    FetchContent_Declare(
        implot
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG        v0.16
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(implot)

    add_library(implot STATIC
        ${implot_SOURCE_DIR}/implot.cpp
        ${implot_SOURCE_DIR}/implot_items.cpp
    )
    target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
    target_link_libraries(implot PUBLIC imgui)
endif()

# ---- SQLite3 (amalgamation — no system install needed) --------------------
FetchContent_Declare(
    sqlite3
    URL      https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
if(UNIX)
    target_link_libraries(sqlite3 PUBLIC pthread ${CMAKE_DL_LIBS})
endif()
# Compile as C, suppress warnings from third-party source
set_target_properties(sqlite3 PROPERTIES LINKER_LANGUAGE C)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()

# ===========================================================================
#  Subdirectories
# ===========================================================================
add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(src/tests)
endif()

message(STATUS "Project source dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Project binary dir: ${CMAKE_BINARY_DIR}")
message(STATUS "BUILD_GUI:          ${BUILD_GUI}")
message(STATUS "BUILD_CLI:          ${BUILD_CLI}")
message(STATUS "BUILD_TESTS:        ${BUILD_TESTS}")
