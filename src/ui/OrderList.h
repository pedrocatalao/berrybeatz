#pragma once
#include "imgui.h"
#include "Theme.h"
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstring>

// OrderList — LCD strip showing pattern order numbers.
//
//   id          — ImGui window ID for the scrolling child window
//   labelFont   — font used for the "Pat" label (may be null)
//   slotFont    — font used for the order numbers (may be null)
//   nOrders     — total number of orders
//   curOrder    — currently playing/selected order index
//   getPattern  — maps order index → pattern number to display
//   onSeek      — called when an order slot is clicked (receives order index)
//   x, y        — top-left of the widget in screen coordinates
//   h           — widget height
//   maxWidth    — maximum allowed width; widget shrinks to fit content if smaller
//   hostFlags   — ImGuiWindowFlags from the host (NoTitleBar, NoResize, etc.)
//
// Width is content-driven: grows to fit all slots, capped at maxWidth.
// Current slot is reversed (green bg, dark text); others show dim green text.
inline void OrderList(const char*   id,
                      ImFont*       labelFont,
                      ImFont*       slotFont,
                      int32_t       nOrders,
                      int32_t       curOrder,
                      const std::function<int32_t(int32_t)>& getPattern,
                      const std::function<void(int32_t)>&    onSeek,
                      float x, float y, float h,
                      float maxWidth,
                      ImGuiWindowFlags hostFlags)
{
    constexpr float R          = 1.5f;
    constexpr float LABEL_W    = 33.f;
    constexpr float PAD_R      = 6.f;
    constexpr float LCD_MARGIN = 4.f;
    constexpr float ITEM_GAP   = 2.f;

    // ── Measure natural content width using the slot font ─────────────────────
    if (slotFont) ImGui::PushFont(slotFont);
    float refW  = ImGui::CalcTextSize("00").x;
    if (slotFont) ImGui::PopFont();

    float slotW      = refW + 10.f;
    float naturalLcd = nOrders > 0
                       ? (nOrders * slotW + (nOrders - 1) * ITEM_GAP)
                       : 80.f;
    float w    = std::min(LABEL_W + naturalLcd + PAD_R, maxWidth);
    float lcdW = w - LABEL_W - PAD_R;
    float LCD_H = h - LCD_MARGIN * 2.f;
    float lcdX  = x + LABEL_W;
    float lcdY  = y + LCD_MARGIN;

    ImDrawList* bgDL = ImGui::GetBackgroundDrawList();
    ImDrawList* fgDL = ImGui::GetForegroundDrawList();
    ImVec2 outerTL = {x,     y};
    ImVec2 outerBR = {x + w, y + h};
    float  bvR = outerBR.x - 1.f, bvB = outerBR.y - 1.f;

    // ── Outer body fill + sheen ───────────────────────────────────────────────
    bgDL->AddRectFilled(outerTL, outerBR, IM_COL32(25, 25, 38, 255), R);
    bgDL->AddRectFilled(outerTL, {outerBR.x, y + h * 0.5f},
                        IM_COL32(255, 255, 255, 14), R);

    // LCD housing shadow + fill (drawn once, explicitly, in background)
    bgDL->AddRectFilled({lcdX - 1.f, lcdY - 1.f},
                        {lcdX + lcdW + 1.f, lcdY + LCD_H + 1.f},
                        IM_COL32(0, 0, 0, 180), 2.f);
    bgDL->AddRectFilled({lcdX, lcdY}, {lcdX + lcdW, lcdY + LCD_H},
                        kClrLcdBg, 1.5f);

    // ── "Pat" label + bevel + border on top (foreground) ─────────────────────
    if (labelFont) {
        float fh = labelFont->LegacySize;
        fgDL->AddText(labelFont, fh, {x + 8.f, y + (h - fh) * 0.5f},
                      kClrTextHint, "Pat");
    }
    fgDL->AddLine({outerTL.x + R,    outerTL.y + 0.5f},
                  {bvR - R,          outerTL.y + 0.5f}, IM_COL32(255,255,255, 55));
    fgDL->AddLine({outerTL.x + 0.5f, outerTL.y + R   },
                  {outerTL.x + 0.5f, bvB             }, IM_COL32(255,255,255, 40));
    fgDL->AddLine({outerTL.x,        bvB             },
                  {bvR,              bvB             }, IM_COL32(0,0,0, 120), 2.f);
    fgDL->AddLine({bvR,              outerTL.y       },
                  {bvR,              bvB             }, IM_COL32(0,0,0, 100));
    fgDL->AddRect(outerTL, outerBR, IM_COL32(30, 30, 45, 255), R, 0, 1.f);

    // ── Scrolling child window (transparent — LCD fill is drawn above) ────────
    ImGui::SetNextWindowPos ({lcdX, lcdY});
    ImGui::SetNextWindowSize({lcdW, LCD_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin(id, nullptr, hostFlags | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (nOrders <= 0) {
        ImGui::SetCursorPosY(2);
        ImGui::TextColored({0.4f, 0.4f, 0.55f, 1.f},
                           "  Open a module file  (File > Open or drag & drop)");
        ImGui::End();
        return;
    }

    if (slotFont) ImGui::PushFont(slotFont);

    float boxH = LCD_H;
    ImGui::SetCursorPosY(0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ITEM_GAP, 0.f});

    ImDrawList*  dl    = ImGui::GetWindowDrawList();
    ImFont*      fnt   = slotFont ? slotFont : ImGui::GetFont();
    ImFontBaked* baked = fnt->GetFontBaked(ImGui::GetFontSize());

    for (int32_t i = 0; i < nOrders; i++) {
        bool    isCur = (i == curOrder);
        int32_t pat   = getPattern(i);

        char sid[16], txt[8];
        snprintf(sid, sizeof(sid), "##o%d", i);
        snprintf(txt, sizeof(txt), "%02d",  pat);

        ImVec2 p   = ImGui::GetCursorScreenPos();
        ImVec2 pBR = {p.x + slotW, p.y + boxH};
        ImGui::InvisibleButton(sid, {slotW, boxH});
        bool hov     = ImGui::IsItemHovered();
        bool pressed = ImGui::IsItemActive();
        if (ImGui::IsItemClicked() && onSeek) onSeek(i);

        if (isCur) ImGui::SetScrollHereX(0.5f);

        if (isCur)
            dl->AddRectFilled({p.x + 1.f, p.y + 1.f}, {pBR.x - 1.f, pBR.y - 1.f},
                              LcdTextA(220), 2.f);
        else if (hov)
            dl->AddRectFilled(p, pBR, LcdTextA(28), 2.f);

        // Center using visual pixel extent (not advance-box width)
        ImVec2 ts = ImGui::CalcTextSize(txt);
        if (baked && txt[0]) {
            const ImFontGlyph* last = baked->FindGlyph((ImWchar)(unsigned char)txt[strlen(txt)-1]);
            if (last) ts.x -= last->AdvanceX - last->X1;
        }
        float nudge  = (pressed && !isCur) ? 1.f : 0.f;
        float tx     = p.x + (slotW - ts.x) * 0.5f + nudge;
        float ty     = p.y + (boxH  - ImGui::CalcTextSize(txt).y) * 0.5f + nudge;
        ImU32 txtCol = isCur ? kClrLcdBg : hov ? LcdTextA(210) : LcdTextA(130);
        dl->AddText({tx, ty}, txtCol, txt);

        ImGui::SameLine(0, ITEM_GAP);
    }

    ImGui::PopStyleVar();
    if (slotFont) ImGui::PopFont();
    ImGui::End();
}
