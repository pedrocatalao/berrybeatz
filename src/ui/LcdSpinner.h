#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "imgui.h"
#include "Theme.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

// LcdSpinner — numeric stepper styled like SwitchButton.
//   Layout: [ Label  [LCD value]  −  + ]
//
//   id    — ImGui string ID
//   label — text shown left of the LCD
//   value — current value (display only; caller owns state)
//   vmin  — minimum value (delta clamped here)
//   vmax  — maximum value
//   size  — optional fixed size; {0,0} = auto from content
//
// Returns: −1 (dec clicked), +1 (inc clicked), 0 (no change).
inline int LcdSpinner(const char* id, const char* label,
                      int value, int vmin, int vmax,
                      ImVec2 size = {0.f, 0.f})
{
    ImGui::PushID(id);

    constexpr float ROUNDING = 1.5f;
    const     float PAD_X    = ImGui::GetStyle().FramePadding.x;
    const     float PAD_Y    = ImGui::GetStyle().FramePadding.y;
    constexpr float LCD_GAP  = 6.f;   // label → LCD housing
    constexpr float CTRL_GAP = 4.f;   // LCD housing → − zone
    constexpr float CTRL_W   = 14.f;  // each − / + tap zone
    constexpr float PAD_R    = 4.f;   // right padding inside + zone

    // Size the LCD by the widest possible value string
    char refBuf[12];
    snprintf(refBuf, sizeof(refBuf), "%d",
             std::max(std::abs(vmin), std::abs(vmax)));
    ImVec2 refSz = ImGui::CalcTextSize(refBuf);
    float  LCD_W = refSz.x + 8.f;

    ImVec2 textSz = ImGui::CalcTextSize(label);
    float  btnH   = (size.y > 0.f) ? size.y : (textSz.y + PAD_Y * 2.f);
    float  LCD_H  = btnH - 8.f;     // leaves 4px frame margin top+bottom

    float mainW  = PAD_X + textSz.x + LCD_GAP + LCD_W + CTRL_GAP;
    float incW   = CTRL_W + PAD_R;
    float totalW = (size.x > 0.f) ? size.x : (mainW + CTRL_W + incW);

    ImVec2 p    = ImGui::GetCursorScreenPos();
    float  decX = p.x + mainW;
    float  incX = decX + CTRL_W;

    // ── Hit zones ─────────────────────────────────────────────────────────
    ImGui::InvisibleButton("##main", {mainW, btnH});
    bool anyHov = ImGui::IsItemHovered();

    ImGui::SameLine(0, 0);
    ImGui::InvisibleButton("##dec", {CTRL_W, btnH});
    bool decHov     = ImGui::IsItemHovered();
    bool decHeld    = ImGui::IsItemActive();
    bool decClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    anyHov |= decHov;

    ImGui::SameLine(0, 0);
    ImGui::InvisibleButton("##inc", {incW, btnH});
    bool incHov     = ImGui::IsItemHovered();
    bool incHeld    = ImGui::IsItemActive();
    bool incClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    anyHov |= incHov;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Outer body ────────────────────────────────────────────────────────
    ImU32 bodyCol = anyHov ? IM_COL32(38, 38, 55, 255) : IM_COL32(25, 25, 38, 255);
    dl->AddRectFilled(p, {p.x + totalW, p.y + btnH}, bodyCol, ROUNDING);

    // Per-zone held / hover highlight
    if (decHeld)
        dl->AddRectFilled({decX, p.y}, {incX, p.y + btnH},
                          IM_COL32(50, 50, 70, 255), 0.f);
    else if (decHov)
        dl->AddRectFilled({decX, p.y}, {incX, p.y + btnH},
                          IM_COL32(255, 255, 255, 12), 0.f);

    if (incHeld)
        dl->AddRectFilled({incX, p.y}, {p.x + totalW, p.y + btnH},
                          IM_COL32(50, 50, 70, 255), 0.f);
    else if (incHov)
        dl->AddRectFilled({incX, p.y}, {p.x + totalW, p.y + btnH},
                          IM_COL32(255, 255, 255, 12), 0.f);

    // Top sheen (convex feel)
    dl->AddRectFilled(p, {p.x + totalW, p.y + btnH * 0.5f},
                      IM_COL32(255, 255, 255, 14), ROUNDING);

    // Outer bevel
    dl->AddLine({p.x + ROUNDING,          p.y + 0.5f},
                {p.x + totalW - ROUNDING, p.y + 0.5f},
                IM_COL32(255, 255, 255, 55));
    dl->AddLine({p.x + 0.5f, p.y + ROUNDING},
                {p.x + 0.5f, p.y + btnH - ROUNDING},
                IM_COL32(255, 255, 255, 40));
    dl->AddLine({p.x + ROUNDING,          p.y + btnH - 0.5f},
                {p.x + totalW - ROUNDING, p.y + btnH - 0.5f},
                IM_COL32(0, 0, 0, 120));
    dl->AddLine({p.x + totalW - 0.5f, p.y + ROUNDING},
                {p.x + totalW - 0.5f, p.y + btnH - ROUNDING},
                IM_COL32(0, 0, 0, 100));

    // Dividers before − and +
    dl->AddLine({decX, p.y + 2.f}, {decX, p.y + btnH - 2.f},
                IM_COL32(255, 255, 255, 30));
    dl->AddLine({incX, p.y + 2.f}, {incX, p.y + btnH - 2.f},
                IM_COL32(255, 255, 255, 30));

    // Outer border
    dl->AddRect(p, {p.x + totalW, p.y + btnH},
                IM_COL32(30, 30, 45, 255), ROUNDING, 0, 1.f);

    // ── Label ─────────────────────────────────────────────────────────────
    float textY = p.y + (btnH - textSz.y) * 0.5f;
    dl->AddText({p.x + PAD_X, textY}, kClrTextLabel, label);

    // ── LCD housing ───────────────────────────────────────────────────────
    float lcdX = p.x + PAD_X + textSz.x + LCD_GAP;
    float lcdY = p.y + (btnH - LCD_H) * 0.5f;

    dl->AddRectFilled({lcdX - 1.f, lcdY - 1.f},
                      {lcdX + LCD_W + 1.f, lcdY + LCD_H + 1.f},
                      IM_COL32(0, 0, 0, 180), 2.f);
    dl->AddRectFilled({lcdX, lcdY}, {lcdX + LCD_W, lcdY + LCD_H},
                      IM_COL32(10, 30, 40, 255), 1.5f);

    char   valBuf[12];
    snprintf(valBuf, sizeof(valBuf), "%d", value);
    ImVec2 valSz = ImGui::CalcTextSize(valBuf);
    dl->PushClipRect({lcdX, lcdY}, {lcdX + LCD_W, lcdY + LCD_H}, true);
    dl->AddText({lcdX + (LCD_W - valSz.x) * 0.5f,
                 lcdY + (LCD_H - valSz.y) * 0.5f},
                IM_COL32(100, 220, 160, 255), valBuf);
    dl->PopClipRect();

    // ── − symbol ──────────────────────────────────────────────────────────
    {
        float  nd  = decHeld ? 1.f : 0.f;
        ImVec2 sym = ImGui::CalcTextSize("-");
        dl->AddText({decX + (CTRL_W - sym.x) * 0.5f + nd,
                     p.y  + (btnH  - sym.y) * 0.5f + nd},
                    kClrTextLabel, "-");
    }

    // ── + symbol ──────────────────────────────────────────────────────────
    {
        float  ni  = incHeld ? 1.f : 0.f;
        ImVec2 sym = ImGui::CalcTextSize("+");
        dl->AddText({incX + (CTRL_W - sym.x) * 0.5f + ni,
                     p.y  + (btnH  - sym.y) * 0.5f + ni},
                    kClrTextLabel, "+");
    }

    ImGui::PopID();

    int delta = decClicked ? -1 : incClicked ? +1 : 0;
    if (delta < 0 && value <= vmin) delta = 0;
    if (delta > 0 && value >= vmax) delta = 0;
    return delta;
}
