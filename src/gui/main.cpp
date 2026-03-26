/**
 * @file main.cpp
 * @brief Entry point for the ImGui + ImPlot resource monitor GUI.
 *
 * Sets up a GLFW window with OpenGL 3.3, initialises ImGui and ImPlot,
 * applies the dark theme, and runs the application main loop.
 */

#include "app.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// ---------------------------------------------------------------------------
//  GLFW error callback
// ---------------------------------------------------------------------------
static void glfwErrorCB(int err, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", err, desc);
}

// ---------------------------------------------------------------------------
//  App::init — create window, GL context, ImGui/ImPlot contexts
// ---------------------------------------------------------------------------
bool App::init() {
    glfwSetErrorCallback(glfwErrorCB);
    if (!glfwInit()) return false;

    // Query primary monitor to size window at 60% of screen
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int winW = static_cast<int>(mode->width  * 0.6f);
    int winH = static_cast<int>(mode->height * 0.6f);

    // Get DPI scale for the monitor
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    float dpiScale = (xscale > yscale) ? xscale : yscale;
    if (dpiScale < 1.0f) dpiScale = 1.0f;

    // GL 3.3 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    window_ = glfwCreateWindow(winW, winH, "Resource Monitor", nullptr, nullptr);
    if (!window_) { glfwTerminate(); return false; }

    // Centre the window on screen
    glfwSetWindowPos(window_,
        (mode->width  - winW) / 2,
        (mode->height - winH) / 2);

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // vsync

    // ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "resource_monitor_imgui.ini";

    // Scale font to match DPI
    float fontSize = std::round(15.0f * dpiScale);
    io.Fonts->AddFontDefault();
    ImFontConfig fontCfg;
    fontCfg.SizePixels = fontSize;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    io.FontDefault = io.Fonts->AddFontDefault(&fontCfg);

    // Platform / renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    const char* glslVersion = "#version 330 core";
    ImGui_ImplOpenGL3_Init(glslVersion);

    // Apply dark monitoring theme, then scale all sizes by DPI
    Theme::Apply();
    ImGui::GetStyle().ScaleAllSizes(dpiScale);

    // Initialise core modules
    Logger::initialize("resource_monitor.log");
    Logger::setConsoleOutput(false);

    cpu_     = createCPU();
    memory_  = createMemory();
    network_ = createNetwork();
    disk_    = createDisk();
    gpu_     = createGPU();
    process_ = createProcessManager();

    db_.initialize();

    Logger::log("GUI initialised");
    return true;
}

// ---------------------------------------------------------------------------
//  App::run — main loop
// ---------------------------------------------------------------------------
void App::run() {
    running_ = true;
    collectorThread_ = std::thread(&App::collectorLoop, this);

    while (!glfwWindowShouldClose(window_) && running_) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render();

        ImGui::Render();
        int fbW, fbH;
        glfwGetFramebufferSize(window_, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.10f, 0.10f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }

    running_ = false;  // signal collector to stop
}

// ---------------------------------------------------------------------------
//  App::shutdown
// ---------------------------------------------------------------------------
void App::shutdown() {
    running_ = false;
    if (collectorThread_.joinable()) collectorThread_.join();

    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }

    Logger::log("GUI shutdown complete");
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------
int main(int /*argc*/, char* /*argv*/[]) {
    App app;
    if (!app.init()) {
        fprintf(stderr, "Failed to initialise application.\n");
        return EXIT_FAILURE;
    }
    app.run();
    app.shutdown();
    return EXIT_SUCCESS;
}
