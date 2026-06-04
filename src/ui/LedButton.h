#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "imgui.h"
#include "Theme.h"
#include "RenderSettings.h"

// LedButton — toggle button with a rectangular LED indicator on the right.
//   id      — ImGui string ID
//   label   — button label text
//   on      — current toggle state
//   size    — optional fixed size; {0,0} = auto-sized to content
//   ledCol  — LED colour when on (default cyan)
//
// Returns true if clicked this frame (caller flips state).
inline bool LedButton(const char* id, const char* label, bool on,
                      ImVec2 size   = {0.f, 0.f},
                      ImU32  ledCol = kClrCyan)
{
    ImGui::PushID(id);

    constexpr float ROUNDING  = 2.0f;
    const     float PAD_X     = ImGui::GetStyle().FramePadding.x;
    const     float PAD_Y     = ImGui::GetStyle().FramePadding.y;
    constexpr float LED_W     = 4.0f;
    constexpr float LED_H     = 10.0f;
    constexpr float LED_GAP   = 6.0f;
    constexpr float LED_PAD_R = 6.0f;

    ImVec2 textSz = ImGui::CalcTextSize(label);
    float  btnW   = (size.x > 0.f) ? size.x
                  : (PAD_X + textSz.x + LED_GAP + LED_W + LED_PAD_R);
    float  btnH   = (size.y > 0.f) ? size.y
                  : (textSz.y + PAD_Y * 2.f);

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##btn", {btnW, btnH});
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool hovered = ImGui::IsItemHovered();
    bool held    = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Body — darker than standard toolbar buttons
    ImU32 bodyCol = held    ? IM_COL32(50, 50, 70, 255)
                 : hovered ? IM_COL32(38, 38, 55, 255)
                            : IM_COL32(25, 25, 38, 255);
    dl->AddRectFilled({p.x, p.y}, {p.x + btnW, p.y + btnH}, bodyCol, ROUNDING);

    // Top sheen — subtle lighter wash on upper half for convex feel
    dl->AddRectFilled({p.x, p.y}, {p.x + btnW, p.y + btnH * 0.5f},
                      IM_COL32(255, 255, 255, held ? 0 : 14), ROUNDING);

    // Bevel edges
    if (!held) {
        // Top highlight
        dl->AddLine({p.x + ROUNDING,        p.y + 0.5f},
                    {p.x + btnW - ROUNDING, p.y + 0.5f},
                    IM_COL32(255, 255, 255, 55));
        // Left highlight
        dl->AddLine({p.x + 0.5f, p.y + ROUNDING},
                    {p.x + 0.5f, p.y + btnH - ROUNDING},
                    IM_COL32(255, 255, 255, 40));
        // Bottom shadow
        dl->AddLine({p.x + ROUNDING,        p.y + btnH - 0.5f},
                    {p.x + btnW - ROUNDING, p.y + btnH - 0.5f},
                    IM_COL32(0, 0, 0, 120));
        // Right shadow
        dl->AddLine({p.x + btnW - 0.5f, p.y + ROUNDING},
                    {p.x + btnW - 0.5f, p.y + btnH - ROUNDING},
                    IM_COL32(0, 0, 0, 100));
    } else {
        // Pressed — inverted bevel (sunken look)
        dl->AddLine({p.x + ROUNDING,        p.y + 0.5f},
                    {p.x + btnW - ROUNDING, p.y + 0.5f},
                    IM_COL32(0, 0, 0, 100));
        dl->AddLine({p.x + 0.5f, p.y + ROUNDING},
                    {p.x + 0.5f, p.y + btnH - ROUNDING},
                    IM_COL32(0, 0, 0, 80));
        dl->AddLine({p.x + ROUNDING,        p.y + btnH - 0.5f},
                    {p.x + btnW - ROUNDING, p.y + btnH - 0.5f},
                    IM_COL32(255, 255, 255, 35));
        dl->AddLine({p.x + btnW - 0.5f, p.y + ROUNDING},
                    {p.x + btnW - 0.5f, p.y + btnH - ROUNDING},
                    IM_COL32(255, 255, 255, 25));
    }

    // Border
    dl->AddRect({p.x, p.y}, {p.x + btnW, p.y + btnH},
                IM_COL32(55, 55, 78, 255), ROUNDING, 0, 1.0f);

    // Label — same color regardless of on/off; nudge 1px down-right when pressed
    float nudge = held ? 1.f : 0.f;
    float textY = p.y + (btnH - textSz.y) * 0.5f + nudge;
    dl->AddText({p.x + PAD_X + nudge, textY}, kClrTextLabel, label);

    // LED position — nudge with press
    float ledX = p.x + btnW - LED_PAD_R - LED_W + nudge;
    float ledY = p.y + (btnH - LED_H) * 0.5f + nudge;

    // LED housing (recessed slot)
    dl->AddRectFilled({ledX - 1.f, ledY - 1.f},
                      {ledX + LED_W + 1.f, ledY + LED_H + 1.f},
                      IM_COL32(0, 0, 0, 150), 2.0f);

    if (on) {
        // Glow halo — additive blend, scaled by g_glowKnobs
        if (g_glowKnobs > 0.f) {
            auto ga = [](int a) { return (uint8_t)(a * g_glowKnobs); };
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }, nullptr);
            ImU32 ledR = (ledCol >> IM_COL32_R_SHIFT) & 0xFF;
            ImU32 ledG = (ledCol >> IM_COL32_G_SHIFT) & 0xFF;
            ImU32 ledB = (ledCol >> IM_COL32_B_SHIFT) & 0xFF;
            constexpr float E1 = 5.f, E2 = 2.5f;
            dl->AddRectFilled({ledX - E1, ledY - E1},
                              {ledX + LED_W + E1, ledY + LED_H + E1},
                              IM_COL32(ledR, ledG, ledB, ga(10)), ROUNDING + E1);
            dl->AddRectFilled({ledX - E2, ledY - E2},
                              {ledX + LED_W + E2, ledY + LED_H + E2},
                              IM_COL32(ledR, ledG, ledB, ga(28)), 2.0f + E2);
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }, nullptr);
        }
        // LED core
        dl->AddRectFilled({ledX, ledY}, {ledX + LED_W, ledY + LED_H}, ledCol, 1.5f);
    } else {
        // LED off — dark tinted
        dl->AddRectFilled({ledX, ledY}, {ledX + LED_W, ledY + LED_H},
                          IM_COL32(10, 30, 40, 255), 1.5f);
    }

    ImGui::PopID();
    return clicked;
}
