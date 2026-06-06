#pragma once
#include "imgui.h"
#include "Theme.h"
#include <cfloat>
#include <cmath>

// LcdDropdown — styled dropdown matching LcdSpinner/SwitchButton aesthetics.
//   Layout: [ Label  [LCD text         ▼] ]
//
//   id        — ImGui string ID
//   label     — text shown left of the LCD
//   selected  — in/out: 0-based index into items[]; -1 if noneLabel is active
//   items     — array of C-strings
//   count     — number of items
//   noneLabel — optional "no selection" entry shown at index -1
//   lcdWidth  — width of the LCD text area (default 130)
//
// Returns true if the selection changed this frame.
inline bool LcdDropdown(const char* id, const char* label,
                        int* selected,
                        const char* const* items, int count,
                        const char* noneLabel = nullptr,
                        float lcdWidth = 130.f)
{
    ImGui::PushID(id);

    constexpr float ROUNDING  = 1.5f;
    const     float PAD_X     = ImGui::GetStyle().FramePadding.x;
    const     float PAD_Y     = ImGui::GetStyle().FramePadding.y;
    constexpr float LCD_GAP   = 6.f;
    constexpr float ARROW_W   = 22.f;
    constexpr float PAD_R     = 2.f;
    constexpr float BTN_M     = 2.f;
    constexpr float LCD_PAD   = 4.f;

    ImVec2 textSz = ImGui::CalcTextSize(label);
    float  btnH   = textSz.y + PAD_Y * 2.f;
    float  LCD_H  = btnH - 8.f;

    float lcdX   = PAD_X + textSz.x + LCD_GAP;  // relative to p.x
    float totalW = lcdX + lcdWidth + ARROW_W + PAD_R;
    float arrowX = lcdX + lcdWidth;              // relative to p.x

    ImVec2 p       = ImGui::GetCursorScreenPos();
    ImVec2 widgetBR = {p.x + totalW, p.y + btnH};

    ImGui::InvisibleButton("##main", {lcdX + lcdWidth, btnH});
    bool mainHov   = ImGui::IsItemHovered();
    bool mainClick = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    ImGui::SameLine(0, 0);
    ImGui::InvisibleButton("##arrow", {ARROW_W + PAD_R, btnH});
    bool arrowHov   = ImGui::IsItemHovered();
    bool arrowHeld  = ImGui::IsItemActive();
    bool arrowClick = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    bool anyHov = mainHov || arrowHov;
    if (mainClick || arrowClick) ImGui::OpenPopup("##dd");

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Outer body ───────────────────────────────────────────────────────────
    ImU32 bodyCol = anyHov ? IM_COL32(38, 38, 55, 255) : IM_COL32(25, 25, 38, 255);
    dl->AddRectFilled(p, widgetBR, bodyCol, ROUNDING);

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
    dl->AddRect(p, widgetBR, IM_COL32(30, 30, 45, 255), ROUNDING, 0, 1.f);

    // ── Label ────────────────────────────────────────────────────────────────
    dl->AddText({p.x + PAD_X, p.y + (btnH - textSz.y) * 0.5f}, kClrTextLabel, label);

    // ── LCD housing ──────────────────────────────────────────────────────────
    float lcdAbsX = p.x + lcdX;
    float lcdAbsY = p.y + (btnH - LCD_H) * 0.5f;

    dl->AddRectFilled({lcdAbsX - 1.f, lcdAbsY - 1.f},
                      {lcdAbsX + lcdWidth + 1.f, lcdAbsY + LCD_H + 1.f},
                      IM_COL32(0, 0, 0, 180), 2.f);
    dl->AddRectFilled({lcdAbsX, lcdAbsY},
                      {lcdAbsX + lcdWidth, lcdAbsY + LCD_H},
                      kClrLcdBg, 1.5f);

    const char* preview = (noneLabel && *selected < 0) ? noneLabel
                        : (*selected >= 0 && *selected < count) ? items[*selected]
                        : "";
    ImVec2 previewSz  = ImGui::CalcTextSize(preview);
    float  availW     = lcdWidth - LCD_PAD * 2.f;
    float  scrollOff  = 0.f;
    if (previewSz.x > availW) {
        constexpr float SPEED = 30.f;   // px/s
        constexpr float PAUSE = 1.5f;   // s pause at each end
        float overflow   = previewSz.x - availW;
        float tScroll    = overflow / SPEED;
        float period     = 2.f * (tScroll + PAUSE);
        float t          = std::fmod((float)ImGui::GetTime(), period);
        if      (t < PAUSE)                    scrollOff = 0.f;
        else if (t < PAUSE + tScroll)          scrollOff = (t - PAUSE) * SPEED;
        else if (t < 2.f * PAUSE + tScroll)    scrollOff = overflow;
        else                                   scrollOff = overflow - (t - 2.f * PAUSE - tScroll) * SPEED;
    }
    dl->PushClipRect({lcdAbsX, lcdAbsY}, {lcdAbsX + lcdWidth, lcdAbsY + LCD_H}, true);
    dl->AddText({lcdAbsX + LCD_PAD - scrollOff, lcdAbsY + (LCD_H - previewSz.y) * 0.5f},
                kClrLcdText, preview);
    dl->PopClipRect();

    // ── Arrow button ─────────────────────────────────────────────────────────
    {
        float  arrowAbsX = p.x + arrowX;
        ImVec2 bTL = {arrowAbsX + BTN_M,           p.y + BTN_M};
        ImVec2 bBR = {p.x + totalW - PAD_R - BTN_M, p.y + btnH - BTN_M};

        ImU32 bCol = arrowHeld ? IM_COL32(18, 18, 30, 255)
                   : arrowHov  ? IM_COL32(48, 48, 68, 255)
                                : IM_COL32(35, 35, 52, 255);
        dl->AddRectFilled(bTL, bBR, bCol, 1.f);
        if (!arrowHeld)
            dl->AddRectFilled(bTL, {bBR.x, bTL.y + (bBR.y - bTL.y) * 0.5f},
                              IM_COL32(255, 255, 255, 10), 1.f);

        if (!arrowHeld) {
            dl->AddLine(bTL,              {bBR.x, bTL.y}, IM_COL32(255,255,255,  8));
            dl->AddLine(bTL,              {bTL.x, bBR.y}, IM_COL32(255,255,255,  6));
            dl->AddLine({bTL.x, bBR.y},  bBR,            IM_COL32(0, 0, 0, 16));
            dl->AddLine({bBR.x, bTL.y},  bBR,            IM_COL32(0, 0, 0, 12));
        } else {
            dl->AddLine(bTL,              {bBR.x, bTL.y}, IM_COL32(0, 0, 0, 110));
            dl->AddLine(bTL,              {bTL.x, bBR.y}, IM_COL32(0, 0, 0,  90));
            dl->AddLine({bTL.x, bBR.y},  bBR,            IM_COL32(255,255,255, 55));
            dl->AddLine({bBR.x, bTL.y},  bBR,            IM_COL32(255,255,255, 40));
        }

        float na = arrowHeld ? 1.f : 0.f;
        float cx = arrowAbsX + ARROW_W * 0.5f + na;
        float cy = p.y + btnH * 0.5f + na;
        constexpr float S = 4.f;
        dl->AddTriangleFilled(
            {cx - S, cy - S * 0.55f}, {cx + S, cy - S * 0.55f}, {cx, cy + S * 0.65f},
            kClrTextLabel);
    }

    // ── Popup ────────────────────────────────────────────────────────────────
    bool changed = false;
    ImGui::SetNextWindowPos({p.x, p.y + btnH}, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints({totalW, 0.f}, {totalW, FLT_MAX});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4.f, 4.f});
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(20, 20, 32, 245));
    if (ImGui::BeginPopup("##dd")) {
        if (noneLabel) {
            bool sel = (*selected < 0);
            if (ImGui::Selectable(noneLabel, sel) && !sel) { *selected = -1; changed = true; }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        for (int i = 0; i < count; ++i) {
            bool sel = (*selected == i);
            if (ImGui::Selectable(items[i], sel) && !sel) { *selected = i; changed = true; }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::PopID();
    return changed;
}
