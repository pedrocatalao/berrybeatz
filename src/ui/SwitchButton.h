#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "imgui.h"
#include "Theme.h"
#include "RenderSettings.h"

// SwitchButton — shared button widget for all toolbar/panel buttons.
//
//   id      — ImGui string ID
//   label   — displayed text
//   value   — current state (0 = off/default); ignored when levels=0
//   levels  — 0: plain button (no LED)
//              1: binary LED (on/off), value is 0 or 1
//              N: N-segment LED, value 0..N lights 0..N segments bottom-to-top
//   size    — optional fixed size; {0,0} = auto-sized to content
//   ledCol  — LED colour when lit (default cyan)
//
// Returns true if clicked this frame. Caller manages state transitions.
inline bool SwitchButton(const char* id, const char* label,
                         int    value  = 0,
                         int    levels = 0,
                         ImVec2 size   = {0.f, 0.f},
                         ImU32  ledCol = kClrCyan)
{
    ImGui::PushID(id);

    constexpr float ROUNDING  = 1.5f;
    const     float PAD_X     = ImGui::GetStyle().FramePadding.x;
    const     float PAD_Y     = ImGui::GetStyle().FramePadding.y;
    constexpr float LED_W     = 4.0f;
    constexpr float LED_H     = 10.0f;
    constexpr float LED_GAP   = 6.0f;
    constexpr float LED_PAD_R = 6.0f;

    const bool hasLed = levels > 0;

    ImVec2 textSz = ImGui::CalcTextSize(label);
    float  btnW   = (size.x > 0.f) ? size.x
                  : hasLed ? (PAD_X + textSz.x + LED_GAP + LED_W + LED_PAD_R)
                           : (PAD_X * 2.f + textSz.x);
    float  btnH   = (size.y > 0.f) ? size.y : (textSz.y + PAD_Y * 2.f);

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##btn", {btnW, btnH});
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool hovered = ImGui::IsItemHovered();
    bool held    = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Body
    ImU32 bodyCol = held    ? IM_COL32(50, 50, 70, 255)
                 : hovered ? IM_COL32(38, 38, 55, 255)
                            : IM_COL32(25, 25, 38, 255);
    dl->AddRectFilled({p.x, p.y}, {p.x + btnW, p.y + btnH}, bodyCol, ROUNDING);

    // Top sheen — convex feel, removed when pressed
    if (!held)
        dl->AddRectFilled({p.x, p.y}, {p.x + btnW, p.y + btnH * 0.5f},
                          IM_COL32(255, 255, 255, 14), ROUNDING);

    // Bevel
    if (!held) {
        dl->AddLine({p.x + ROUNDING,        p.y + 0.5f},
                    {p.x + btnW - ROUNDING, p.y + 0.5f},   IM_COL32(255,255,255, 55));
        dl->AddLine({p.x + 0.5f, p.y + ROUNDING},
                    {p.x + 0.5f, p.y + btnH - ROUNDING},   IM_COL32(255,255,255, 40));
        dl->AddLine({p.x + ROUNDING,        p.y + btnH - 0.5f},
                    {p.x + btnW - ROUNDING, p.y + btnH - 0.5f}, IM_COL32(0,0,0, 120));
        dl->AddLine({p.x + btnW - 0.5f, p.y + ROUNDING},
                    {p.x + btnW - 0.5f, p.y + btnH - ROUNDING}, IM_COL32(0,0,0, 100));
    } else {
        dl->AddLine({p.x + ROUNDING,        p.y + 0.5f},
                    {p.x + btnW - ROUNDING, p.y + 0.5f},   IM_COL32(0,0,0, 100));
        dl->AddLine({p.x + 0.5f, p.y + ROUNDING},
                    {p.x + 0.5f, p.y + btnH - ROUNDING},   IM_COL32(0,0,0, 80));
        dl->AddLine({p.x + ROUNDING,        p.y + btnH - 0.5f},
                    {p.x + btnW - ROUNDING, p.y + btnH - 0.5f}, IM_COL32(255,255,255, 35));
        dl->AddLine({p.x + btnW - 0.5f, p.y + ROUNDING},
                    {p.x + btnW - 0.5f, p.y + btnH - ROUNDING}, IM_COL32(255,255,255, 25));
    }

    // Border
    dl->AddRect({p.x, p.y}, {p.x + btnW, p.y + btnH},
                IM_COL32(30, 30, 45, 255), ROUNDING, 0, 1.0f);

    // Label — nudge 1px when pressed
    float nudge = held ? 1.f : 0.f;
    float textY = p.y + (btnH - textSz.y) * 0.5f + nudge;
    dl->AddText({p.x + PAD_X + nudge, textY}, ImGui::GetColorU32(kClrTextLabel), label);

    // LED
    if (hasLed) {
        float ledX = p.x + btnW - LED_PAD_R - LED_W + nudge;
        float ledY = p.y + (btnH - LED_H) * 0.5f + nudge;

        // Housing (recessed slot)
        dl->AddRectFilled({ledX - 1.f, ledY - 1.f},
                          {ledX + LED_W + 1.f, ledY + LED_H + 1.f},
                          IM_COL32(0, 0, 0, 150), 2.0f);

        // Glow halo when any segment is lit — additive blend
        if (value > 0 && g_glowKnobs > 0.f) {
            auto ga = [](int a) { return (uint8_t)(a * g_glowKnobs); };
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }, nullptr);
            ImU32 r = (ledCol >> IM_COL32_R_SHIFT) & 0xFF;
            ImU32 g = (ledCol >> IM_COL32_G_SHIFT) & 0xFF;
            ImU32 b = (ledCol >> IM_COL32_B_SHIFT) & 0xFF;
            constexpr float E1 = 5.f, E2 = 2.5f;
            dl->AddRectFilled({ledX - E1, ledY - E1},
                              {ledX + LED_W + E1, ledY + LED_H + E1},
                              IM_COL32(r, g, b, ga(10)), ROUNDING + E1);
            dl->AddRectFilled({ledX - E2, ledY - E2},
                              {ledX + LED_W + E2, ledY + LED_H + E2},
                              IM_COL32(r, g, b, ga(28)), 2.0f + E2);
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }, nullptr);
        }

        // Segments — bottom-to-top, lit count = value
        int   clampedVal = value  < 0      ? 0
                         : value  > levels ? levels : value;
        float gapH  = 0.0f;
        float segH  = LED_H / levels;

        for (int i = 0; i < levels; i++) {
            float sy     = ledY + LED_H - (i + 1) * segH - i * gapH;
            bool  lit    = i < clampedVal;
            ImU32 segCol = lit ? ledCol : kClrLcdBg;
            dl->AddRectFilled({ledX, sy}, {ledX + LED_W, sy + segH}, segCol, 1.0f);
        }
    }

    ImGui::PopID();
    return clicked;
}
