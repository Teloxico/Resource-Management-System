# src/gui/CMakeLists.txt — ImGui + ImPlot GUI application

add_executable(ResourceMonitorGUI main.cpp)

target_include_directories(ResourceMonitorGUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ResourceMonitorGUI PRIVATE
    ResourceCore
    Utils
    implot      # pulls in imgui -> glfw -> OpenGL
)
