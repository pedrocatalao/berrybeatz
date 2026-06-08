#pragma once
#include "imgui.h"
#include "Theme.h"

// LcdInput — styled text field matching LcdDropdown/LcdSpinner aesthetics.
//   Layout: [ Label  [LCD input area] ]
//
//   id       — ImGui string ID
//   label    — text shown left of the input area
//   buf      — text buffer (read/write)
//   bufSize  — capacity of buf
//   lcdWidth — width of the input area (default 120)
//   flags    — forwarded to InputText
//
// Returns true when the value was edited (same semantics as InputText).
inline bool LcdInput(const char* id, const char* label,
                     char* buf, int bufSize,
                     float lcdWidth = 120.f,
                     ImGuiInputTextFlags flags = 0)
{
    ImGui::PushID(id);

    constexpr float ROUNDING = 1.5f;
    const     float PAD_X    = ImGui::GetStyle().FramePadding.x;
    const     float PAD_Y    = ImGui::GetStyle().FramePadding.y;
    constexpr float LCD_GAP  = 6.f;
    constexpr float LCD_PAD  = 4.f;
    constexpr float PAD_R    = 6.f;

    ImVec2 labelSz = ImGui::CalcTextSize(label);
    float  btnH    = labelSz.y + PAD_Y * 2.f;
    float  lcdRelX = PAD_X + labelSz.x + LCD_GAP;
    float  totalW  = lcdRelX + lcdWidth + PAD_R;

    float  LCD_H   = btnH - 8.f;   // 4px inset top+bottom, matching LcdDropdown

    ImVec2 p        = ImGui::GetCursorScreenPos();
    ImVec2 widgetBR = {p.x + totalW, p.y + btnH};
    float  lcdAbsX  = p.x + lcdRelX;
    float  lcdAbsY  = p.y + (btnH - LCD_H) * 0.5f;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Pre-draw: outer body and LCD inset shadow (drawn under the InputText) ──
    dl->AddRectFilledMultiColor(p, widgetBR,
        IM_COL32(33, 33, 48, 255), IM_COL32(33, 33, 48, 255),
        IM_COL32(14, 14, 26, 255), IM_COL32(14, 14, 26, 255));
    dl->AddLine({lcdAbsX + 0.5f,            lcdAbsY + 0.5f},         {lcdAbsX + lcdWidth - 0.5f, lcdAbsY + 0.5f},         IM_COL32(0,0,0,100));
    dl->AddLine({lcdAbsX + 0.5f,            lcdAbsY + 0.5f},         {lcdAbsX + 0.5f,            lcdAbsY + LCD_H - 0.5f}, IM_COL32(0,0,0,80));
    dl->AddLine({lcdAbsX + 0.5f,            lcdAbsY + LCD_H - 0.5f},{lcdAbsX + lcdWidth - 0.5f,  lcdAbsY + LCD_H - 0.5f},IM_COL32(255,255,255,50));
    dl->AddLine({lcdAbsX + lcdWidth - 0.5f, lcdAbsY + 0.5f},        {lcdAbsX + lcdWidth - 0.5f,  lcdAbsY + LCD_H - 0.5f},IM_COL32(255,255,255,40));

    // ── Group so the shorter InputText doesn't shrink the layout line height ──
    ImGui::BeginGroup();

    // InvisibleButton reserves the label zone at full btnH
    ImGui::InvisibleButton("##lbl", {lcdRelX, btnH});
    ImGui::SameLine(0, 0);

    // ── Styled InputText inset to LCD_H, centered vertically ─────────────────
    float fpY = (LCD_H - labelSz.y) * 0.5f;
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        kClrLcdBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(12, 36, 50, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(12, 36, 50, 255));
    ImGui::PushStyleColor(ImGuiCol_Text,           kClrLcdText);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, IM_COL32(50, 140, 100, 120));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  {LCD_PAD, fpY});
    ImGui::SetCursorScreenPos({lcdAbsX, lcdAbsY});
    ImGui::SetNextItemWidth(lcdWidth);
    bool changed = ImGui::InputText("##inp", buf, bufSize, flags);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(6);

    // Extend group bounding box to cover PAD_R visual right margin,
    // so callers get the correct item rect width for layout/gap decisions.
    ImGui::SameLine(0, 0);
    ImGui::Dummy({PAD_R, LCD_H});

    ImGui::EndGroup();

    // ── Post-draw: label and outer bevel drawn on top of InputText ──────────────
    dl->AddText({p.x + PAD_X, p.y + (btnH - labelSz.y) * 0.5f},
                ImGui::GetColorU32(kClrTextLabel), label);

    dl->AddLine({p.x + ROUNDING,          p.y + 0.5f},
                {p.x + totalW - ROUNDING, p.y + 0.5f},        IM_COL32(255,255,255, 55));
    dl->AddLine({p.x + 0.5f, p.y + ROUNDING},
                {p.x + 0.5f, p.y + btnH - ROUNDING},          IM_COL32(255,255,255, 40));
    dl->AddLine({p.x + ROUNDING,          p.y + btnH - 0.5f},
                {p.x + totalW - ROUNDING, p.y + btnH - 0.5f}, IM_COL32(0,0,0, 120));
    dl->AddLine({p.x + totalW - 0.5f, p.y + ROUNDING},
                {p.x + totalW - 0.5f, p.y + btnH - ROUNDING}, IM_COL32(0,0,0, 100));
    dl->AddRect(p, widgetBR, IM_COL32(30, 30, 45, 255), ROUNDING, 0, 1.f);

    ImGui::PopID();
    return changed;
}
