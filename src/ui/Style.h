#pragma once
#include "Theme.h"
#include "imgui.h"
#include <functional>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// BerryBeatz application style — ImGui global style and font setup.
// Call applyImGuiStyle() once after ImGui::CreateContext().
// Call loadFonts() once after, passing a bundleResource resolver.
// ─────────────────────────────────────────────────────────────────────────────

inline void applyImGuiStyle()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style          = ImGui::GetStyle();
    style.WindowRounding       = kStyleWindowRounding;
    style.FrameRounding        = kStyleFrameRounding;
    style.ScrollbarRounding    = kStyleScrollbarRounding;
    style.TabRounding          = kStyleTabRounding;
    style.WindowBorderSize     = 0.f;
    style.FrameBorderSize      = 0.f;
    style.Colors[ImGuiCol_WindowBg]      = kStyleWindowBg;
    style.Colors[ImGuiCol_TitleBg]       = kStyleTitleBg;
    style.Colors[ImGuiCol_TitleBgActive] = kStyleTitleBgAct;
}

inline void loadFonts(ImGuiIO& io,
                      const std::function<std::string(const char*)>& bundleRes)
{
    ImFontConfig pixCfg;
    pixCfg.OversampleH = 1;
    pixCfg.OversampleV = 1;
    pixCfg.PixelSnapH  = true;

    ImFontConfig smCfg;
    smCfg.OversampleH = 2;
    smCfg.OversampleV = 2;

    auto load = [&](const char* file, float size, bool smooth) -> ImFont* {
        std::string path = bundleRes(file);
        if (path.empty()) return nullptr;
        return io.Fonts->AddFontFromFileTTF(path.c_str(), size, smooth ? &smCfg : &pixCfg);
    };

    ImFont* michroma13 = load("Michroma.ttf", 13.f, true);
    if (!michroma13) michroma13 = io.Fonts->AddFontDefault();

    ImFont* px2_8  = load("35px2.ttf",  8.f, false);
    ImFont* px2_9  = load("35px2.ttf",  9.f, false);
    ImFont* px2_10 = load("35px2.ttf", 10.f, false);
    ImFont* px2_11 = load("35px2.ttf", 11.f, false);
    if (!px2_8)  px2_8  = io.Fonts->AddFontDefault();
    if (!px2_9)  px2_9  = px2_8;
    if (!px2_10) px2_10 = px2_8;
    if (!px2_11) px2_11 = px2_8;

    ImFont* term11 = load("terminal-6-mono.ttf", 11.f, false);
    if (!term11) term11 = io.Fonts->AddFontDefault();

    io.Fonts->Build();

    g_fontBtnTab    = michroma13;
    g_fontKnobLabel = michroma13;
    g_fontKnobValue = px2_8;
    g_fontRowNum    = px2_11;
    g_fontTracker   = term11;
    g_fontOrderRow  = px2_10;
    g_fontSidebar   = px2_9;
}
