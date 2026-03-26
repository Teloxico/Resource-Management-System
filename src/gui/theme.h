/**
 * @file theme.h
 * @brief Dark theme colours and severity palette for the resource monitor GUI.
 *
 * Colour philosophy:
 *   - Dark background (#1a1a2e) with muted panels for depth.
 *   - Bright accent colours that pop against dark surfaces.
 *   - Five-step severity gradient: green -> yellow-green -> orange -> red-orange -> red.
 */

#pragma once

#include <cstdint>
#include <cstdio>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"

namespace Theme {

// ---- Severity palette (maps 0-100 % to a colour) -------------------------

inline ImVec4 SeverityColor(float pct) {
    if (pct < 25.0f) return ImVec4(0.30f, 0.69f, 0.31f, 1.0f); // green
    if (pct < 50.0f) return ImVec4(0.55f, 0.76f, 0.29f, 1.0f); // yellow-green
    if (pct < 75.0f) return ImVec4(1.00f, 0.60f, 0.00f, 1.0f); // orange
    if (pct < 90.0f) return ImVec4(1.00f, 0.34f, 0.13f, 1.0f); // red-orange
    return              ImVec4(0.96f, 0.26f, 0.21f, 1.0f);       // red
}

// ---- Accent colours -------------------------------------------------------

inline constexpr ImVec4 AccentBlue    {0.26f, 0.59f, 0.98f, 1.0f};
inline constexpr ImVec4 AccentCyan    {0.00f, 0.74f, 0.83f, 1.0f};
inline constexpr ImVec4 AccentGreen   {0.30f, 0.69f, 0.31f, 1.0f};
inline constexpr ImVec4 AccentOrange  {1.00f, 0.60f, 0.00f, 1.0f};
inline constexpr ImVec4 AccentRed     {0.96f, 0.26f, 0.21f, 1.0f};
inline constexpr ImVec4 AccentPurple  {0.61f, 0.32f, 0.88f, 1.0f};
inline constexpr ImVec4 AccentPink    {0.91f, 0.31f, 0.60f, 1.0f};
inline constexpr ImVec4 AccentYellow  {1.00f, 0.84f, 0.00f, 1.0f};

inline constexpr ImVec4 TextPrimary   {0.88f, 0.88f, 0.88f, 1.0f};
inline constexpr ImVec4 TextSecondary {0.55f, 0.55f, 0.65f, 1.0f};
inline constexpr ImVec4 BgDark        {0.10f, 0.10f, 0.18f, 1.0f};
inline constexpr ImVec4 BgPanel       {0.14f, 0.14f, 0.22f, 1.0f};
inline constexpr ImVec4 BgCard        {0.17f, 0.17f, 0.26f, 1.0f};

// ---- Per-core colour cycle ------------------------------------------------

inline ImVec4 CoreColor(int idx) {
    static const ImVec4 palette[] = {
        AccentBlue, AccentCyan, AccentGreen, AccentOrange,
        AccentRed,  AccentPurple, AccentPink, AccentYellow,
        {0.40f,0.85f,0.95f,1}, {0.95f,0.65f,0.40f,1},
        {0.50f,0.90f,0.50f,1}, {0.85f,0.50f,0.95f,1},
        {0.95f,0.85f,0.40f,1}, {0.40f,0.65f,0.95f,1},
        {0.95f,0.40f,0.40f,1}, {0.40f,0.95f,0.75f,1},
    };
    return palette[idx % 16];
}

// ---- Apply full dark theme to ImGui and ImPlot ----------------------------

inline void Apply() {
    // -- ImGui ---------------------------------------------------------------
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 4.0f;
    s.FrameRounding     = 3.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.WindowPadding     = ImVec2(10, 10);
    s.FramePadding      = ImVec2(6, 4);
    s.ItemSpacing       = ImVec2(8, 6);
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 8.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]           = BgDark;
    c[ImGuiCol_ChildBg]            = BgPanel;
    c[ImGuiCol_PopupBg]            = {0.12f, 0.12f, 0.20f, 0.96f};
    c[ImGuiCol_Border]             = {0.22f, 0.22f, 0.32f, 0.50f};
    c[ImGuiCol_FrameBg]            = {0.16f, 0.16f, 0.24f, 1.0f};
    c[ImGuiCol_FrameBgHovered]     = {0.22f, 0.22f, 0.32f, 1.0f};
    c[ImGuiCol_FrameBgActive]      = {0.28f, 0.28f, 0.40f, 1.0f};
    c[ImGuiCol_TitleBg]            = {0.08f, 0.08f, 0.14f, 1.0f};
    c[ImGuiCol_TitleBgActive]      = {0.12f, 0.12f, 0.20f, 1.0f};
    c[ImGuiCol_MenuBarBg]          = {0.11f, 0.11f, 0.18f, 1.0f};
    c[ImGuiCol_ScrollbarBg]        = {0.10f, 0.10f, 0.16f, 0.80f};
    c[ImGuiCol_ScrollbarGrab]      = {0.30f, 0.30f, 0.44f, 1.0f};
    c[ImGuiCol_ScrollbarGrabHovered]={0.40f, 0.40f, 0.56f, 1.0f};
    c[ImGuiCol_ScrollbarGrabActive]= {0.50f, 0.50f, 0.66f, 1.0f};
    c[ImGuiCol_CheckMark]          = AccentBlue;
    c[ImGuiCol_SliderGrab]         = AccentBlue;
    c[ImGuiCol_SliderGrabActive]   = {0.36f, 0.69f, 1.00f, 1.0f};
    c[ImGuiCol_Button]             = {0.20f, 0.20f, 0.32f, 1.0f};
    c[ImGuiCol_ButtonHovered]      = {0.28f, 0.28f, 0.44f, 1.0f};
    c[ImGuiCol_ButtonActive]       = AccentBlue;
    c[ImGuiCol_Header]             = {0.20f, 0.20f, 0.32f, 1.0f};
    c[ImGuiCol_HeaderHovered]      = {0.26f, 0.26f, 0.40f, 1.0f};
    c[ImGuiCol_HeaderActive]       = {0.30f, 0.30f, 0.48f, 1.0f};
    c[ImGuiCol_Tab]                = {0.14f, 0.14f, 0.22f, 1.0f};
    c[ImGuiCol_TabHovered]         = {0.26f, 0.59f, 0.98f, 0.70f};
    c[ImGuiCol_TabSelected]        = {0.20f, 0.41f, 0.68f, 1.0f};
    c[ImGuiCol_TableHeaderBg]      = {0.14f, 0.14f, 0.22f, 1.0f};
    c[ImGuiCol_TableBorderStrong]  = {0.22f, 0.22f, 0.32f, 1.0f};
    c[ImGuiCol_TableBorderLight]   = {0.18f, 0.18f, 0.28f, 1.0f};
    c[ImGuiCol_TableRowBg]         = {0.00f, 0.00f, 0.00f, 0.00f};
    c[ImGuiCol_TableRowBgAlt]      = {1.00f, 1.00f, 1.00f, 0.03f};
    c[ImGuiCol_TextSelectedBg]     = {0.26f, 0.59f, 0.98f, 0.35f};
    c[ImGuiCol_Text]               = TextPrimary;
    c[ImGuiCol_TextDisabled]       = TextSecondary;
    c[ImGuiCol_PlotLines]          = AccentBlue;
    c[ImGuiCol_PlotHistogram]      = AccentOrange;
    c[ImGuiCol_Separator]          = {0.22f, 0.22f, 0.32f, 0.50f};

    // -- ImPlot --------------------------------------------------------------
    ImPlotStyle& ps = ImPlot::GetStyle();
    ps.LineWeight       = 2.0f;
    ps.FillAlpha        = 0.18f;
    ps.PlotPadding      = ImVec2(10, 10);
    ps.LabelPadding     = ImVec2(4, 4);
    ps.FitPadding       = ImVec2(0.05f, 0.05f);
    ps.PlotDefaultSize  = ImVec2(-1, 180);
    ps.PlotMinSize      = ImVec2(200, 100);

    // Use a dark plot background distinct from the window background
    ImVec4* pc = ps.Colors;
    pc[ImPlotCol_PlotBg]        = {0.08f, 0.08f, 0.14f, 1.0f};
    pc[ImPlotCol_PlotBorder]    = {0.22f, 0.22f, 0.32f, 0.50f};
    pc[ImPlotCol_LegendBg]      = {0.12f, 0.12f, 0.20f, 0.90f};
    pc[ImPlotCol_LegendBorder]  = {0.22f, 0.22f, 0.32f, 0.50f};
    pc[ImPlotCol_LegendText]    = TextPrimary;
    pc[ImPlotCol_AxisText]      = TextSecondary;
    pc[ImPlotCol_AxisGrid]      = {0.22f, 0.22f, 0.32f, 0.30f};
    pc[ImPlotCol_AxisBgHovered] = {0.26f, 0.59f, 0.98f, 0.12f};
    pc[ImPlotCol_AxisBgActive]  = {0.26f, 0.59f, 0.98f, 0.20f};
    pc[ImPlotCol_Crosshairs]    = {1.0f, 1.0f, 1.0f, 0.50f};

    // Colourmap for multi-line plots (per-core, per-interface, etc.)
    static const ImVec4 cmap[] = {
        AccentBlue, AccentCyan, AccentGreen, AccentOrange,
        AccentRed,  AccentPurple, AccentPink, AccentYellow,
    };
    ps.Colormap = ImPlot::AddColormap("Monitor", &cmap[0], 8);
}

// ---- Helpers for consistent card drawing ----------------------------------

inline void BeginCard(const char* label, float width = 0, float height = 0) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BgCard);
    ImGui::BeginChild(label, ImVec2(width, height), ImGuiChildFlags_Border,
                      ImGuiWindowFlags_None);
}

inline void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Format byte sizes to human-readable strings
inline const char* FormatBytes(uint64_t bytes, char* buf, size_t bufSize) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double val = static_cast<double>(bytes);
    int u = 0;
    while (val >= 1024.0 && u < 4) { val /= 1024.0; u++; }
    snprintf(buf, bufSize, "%.1f %s", val, units[u]);
    return buf;
}

inline const char* FormatRate(float bytesPerSec, char* buf, size_t bufSize) {
    const char* units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    double val = static_cast<double>(bytesPerSec);
    int u = 0;
    while (val >= 1024.0 && u < 3) { val /= 1024.0; u++; }
    snprintf(buf, bufSize, "%.1f %s", val, units[u]);
    return buf;
}

} // namespace Theme
