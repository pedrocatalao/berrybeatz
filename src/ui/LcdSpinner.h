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

    constexpr float ROUNDING  = 1.5f;
    const     float PAD_X     = ImGui::GetStyle().FramePadding.x;
    const     float PAD_Y     = ImGui::GetStyle().FramePadding.y;
    constexpr float LCD_GAP   = 6.f;    // label → LCD housing
    constexpr float CTRL_GAP  = 6.f;    // LCD housing → control zones
    constexpr float CTRL_W    = 18.f;   // each − / + zone width
    constexpr float PAD_R     = 4.f;    // right padding after + zone
    constexpr float SYM_HALF  = 3.5f;   // symbol arm half-length
    constexpr float SYM_THICK = 0.5f;   // symbol arm half-thickness
    constexpr float BTN_M     = 2.f;    // inset margin for mini buttons

    // LCD sized to widest possible value string
    char refBuf[12];
    snprintf(refBuf, sizeof(refBuf), "%d",
             std::max(std::abs(vmin), std::abs(vmax)));
    ImVec2 refSz = ImGui::CalcTextSize(refBuf);
    float  LCD_W = refSz.x + 8.f;

    ImVec2 textSz = ImGui::CalcTextSize(label);
    float  btnH   = (size.y > 0.f) ? size.y : (textSz.y + PAD_Y * 2.f);
    float  LCD_H  = btnH - 8.f;   // 4px frame margin top+bottom

    float mainW  = PAD_X + textSz.x + LCD_GAP + LCD_W + CTRL_GAP;
    float incW   = CTRL_W + PAD_R;
    float totalW = (size.x > 0.f) ? size.x : (mainW + CTRL_W + incW);

    ImVec2 p    = ImGui::GetCursorScreenPos();
    float  decX = p.x + mainW;
    float  incX = decX + CTRL_W;

    // ── Hit zones ────────────────────────────────────────────────────────
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

    // ── Outer body ───────────────────────────────────────────────────────
    ImU32 bodyCol = anyHov ? IM_COL32(38, 38, 55, 255) : IM_COL32(25, 25, 38, 255);
    dl->AddRectFilled(p, {p.x + totalW, p.y + btnH}, bodyCol, ROUNDING);

    // Top sheen
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

    // Outer border
    dl->AddRect(p, {p.x + totalW, p.y + btnH},
                IM_COL32(30, 30, 45, 255), ROUNDING, 0, 1.f);

    // ── Label ────────────────────────────────────────────────────────────
    float textY = p.y + (btnH - textSz.y) * 0.5f;
    dl->AddText({p.x + PAD_X, textY}, kClrTextLabel, label);

    // ── LCD housing ──────────────────────────────────────────────────────
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

    // ── Dec (−) mini button ──────────────────────────────────────────────
    {
        ImVec2 bTL = {decX + BTN_M,          p.y + BTN_M};
        ImVec2 bBR = {incX - BTN_M,          p.y + btnH - BTN_M};

        ImU32 bCol = decHeld ? IM_COL32(18, 18, 30, 255)
                   : decHov  ? IM_COL32(48, 48, 68, 255)
                              : IM_COL32(35, 35, 52, 255);
        dl->AddRectFilled(bTL, bBR, bCol, 1.f);

        if (!decHeld) {
            dl->AddLine(bTL,              {bBR.x, bTL.y}, IM_COL32(255,255,255, 55));
            dl->AddLine(bTL,              {bTL.x, bBR.y}, IM_COL32(255,255,255, 40));
            dl->AddLine({bTL.x, bBR.y},  bBR,            IM_COL32(0, 0, 0, 110));
            dl->AddLine({bBR.x, bTL.y},  bBR,            IM_COL32(0, 0, 0,  90));
        } else {
            dl->AddLine(bTL,              {bBR.x, bTL.y}, IM_COL32(0, 0, 0, 110));
            dl->AddLine(bTL,              {bTL.x, bBR.y}, IM_COL32(0, 0, 0,  90));
            dl->AddLine({bTL.x, bBR.y},  bBR,            IM_COL32(255,255,255, 55));
            dl->AddLine({bBR.x, bTL.y},  bBR,            IM_COL32(255,255,255, 40));
        }

        float nd = decHeld ? 1.f : 0.f;
        float cx = decX + CTRL_W * 0.5f + nd;
        float cy = p.y  + btnH  * 0.5f + nd;
        dl->AddRectFilled({cx - SYM_HALF,  cy - SYM_THICK},
                          {cx + SYM_HALF,  cy + SYM_THICK}, kClrTextLabel);
    }

    // ── Inc (+) mini button ──────────────────────────────────────────────
    {
        ImVec2 bTL = {incX + BTN_M,              p.y + BTN_M};
        ImVec2 bBR = {p.x + totalW - PAD_R - BTN_M, p.y + btnH - BTN_M};

        ImU32 bCol = incHeld ? IM_COL32(18, 18, 30, 255)
                   : incHov  ? IM_COL32(48, 48, 68, 255)
                              : IM_COL32(35, 35, 52, 255);
        dl->AddRectFilled(bTL, bBR, bCol, 1.f);

        if (!incHeld) {
            dl->AddLine(bTL,              {bBR.x, bTL.y}, IM_COL32(255,255,255, 55));
            dl->AddLine(bTL,              {bTL.x, bBR.y}, IM_COL32(255,255,255, 40));
            dl->AddLine({bTL.x, bBR.y},  bBR,            IM_COL32(0, 0, 0, 110));
            dl->AddLine({bBR.x, bTL.y},  bBR,            IM_COL32(0, 0, 0,  90));
        } else {
            dl->AddLine(bTL,              {bBR.x, bTL.y}, IM_COL32(0, 0, 0, 110));
            dl->AddLine(bTL,              {bTL.x, bBR.y}, IM_COL32(0, 0, 0,  90));
            dl->AddLine({bTL.x, bBR.y},  bBR,            IM_COL32(255,255,255, 55));
            dl->AddLine({bBR.x, bTL.y},  bBR,            IM_COL32(255,255,255, 40));
        }

        float ni = incHeld ? 1.f : 0.f;
        float cx = incX + CTRL_W * 0.5f + ni;
        float cy = p.y  + btnH  * 0.5f + ni;
        dl->AddRectFilled({cx - SYM_HALF,  cy - SYM_THICK},
                          {cx + SYM_HALF,  cy + SYM_THICK}, kClrTextLabel);
        dl->AddRectFilled({cx - SYM_THICK, cy - SYM_HALF},
                          {cx + SYM_THICK, cy + SYM_HALF},  kClrTextLabel);
    }

    ImGui::PopID();

    int delta = decClicked ? -1 : incClicked ? +1 : 0;
    if (delta < 0 && value <= vmin) delta = 0;
    if (delta > 0 && value >= vmax) delta = 0;
    return delta;
}
