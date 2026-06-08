#pragma once
#include "imgui.h"
#include "Theme.h"
#include <functional>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cmath>

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr float kCG_GapH     = 10.f;  // horizontal space between groups
static constexpr float kCG_GapV     =  6.f;  // vertical space between group rows
static constexpr float kCG_PadX     =  7.f;  // border → content (left)
static constexpr float kCG_PadRight = 13.f;  // border → content (right)
static constexpr float kCG_PadTop   = 12.f;  // border → content (top)
static constexpr float kCG_PadBot   =  6.f;  // border → content (bottom)
static constexpr float kCG_ItemGapH =  6.f;  // gap between widgets on same row
static constexpr float kCG_ItemGapV =  4.f;  // gap between widget rows
static constexpr float kCG_CornerR  =  3.f;
static constexpr ImU32 kCG_Border   = IM_COL32(32, 32, 50, 155);
static constexpr ImU32 kCG_LabelCol = IM_COL32(52, 52, 74, 255);

// ── CGroupWidget ──────────────────────────────────────────────────────────────
// width  — declared widget width used for horizontal flow decisions.
// render — draw callback. nullptr = explicit row break (cgNewRow).
struct CGroupWidget {
    float                 width;
    std::function<void()> render;
};

inline CGroupWidget cgNewRow() { return {1e9f, nullptr}; }

// ── ControlGroupDef ───────────────────────────────────────────────────────────
// max_width  — widgets wrap when they exceed this content width.
// max_height — rows stop rendering past this content height.
// Actual rendered size is variable (shrinks to fit content).
struct ControlGroupDef {
    const char*              label;
    float                    max_width;
    float                    max_height;
    ImFont*                  labelFont = nullptr;
    std::vector<CGroupWidget> widgets;
};

// ── DrawControlGroups ─────────────────────────────────────────────────────────
// localX, localY : top-left in current ImGui window's local coordinates.
// availW         : horizontal space before groups wrap to next row.
// Returns the total height (px) used by all group rows — for adaptive window sizing.
inline float DrawControlGroups(float localX, float localY, float availW,
                                const std::vector<ControlGroupDef>& groups)
{
    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      winPos = ImGui::GetWindowPos();
    ImVec2      winPad = ImGui::GetStyle().WindowPadding;

    // Local ↔ screen helpers
    auto lsX = [&](float lx) { return winPos.x + winPad.x + lx; };
    auto lsY = [&](float ly) { return winPos.y + winPad.y + ly; };

    float curX    = localX;
    float curY    = localY;
    float rowMaxH = 0.f;

    for (const auto& g : groups) {
        // Measure label
        float labelH = 0.f, labelW = 0.f;
        if (g.label && g.label[0]) {
            if (g.labelFont) ImGui::PushFont(g.labelFont);
            ImVec2 ls = ImGui::CalcTextSize(g.label);
            labelW = ls.x;  labelH = ls.y;
            if (g.labelFont) ImGui::PopFont();
        }
        float labelMid = std::ceil(labelH * 0.5f);

        // Claimed group width for flow decisions (border width = max_width + padX + padRight)
        float groupW = g.max_width + kCG_PadX + kCG_PadRight;

        // Wrap groups to next row if needed
        if (curX + groupW > localX + availW && curX > localX) {
            curY   += rowMaxH + kCG_GapV;
            curX    = localX;
            rowMaxH = 0.f;
        }

        // Content area dimensions
        float contentLocalX  = curX + kCG_PadX;
        float contentLocalY  = curY + labelH + kCG_PadTop;
        float contentMaxX    = lsX(contentLocalX) + g.max_width;   // screen right limit
        float contentMaxY    = lsY(contentLocalY) + g.max_height;  // screen bottom limit

        // ── Pass 1: measure natural row widths from declared w.width ──────────
        struct RowMeasure { int lastWi; float width; };
        std::vector<RowMeasure> rowMeasures;
        {
            float rowW = 0.f; bool first = true; int lastWi = -1, wi = 0;
            for (const auto& w : g.widgets) {
                if (!w.render) {
                    if (!first) { rowMeasures.push_back({lastWi, rowW}); rowW = 0.f; first = true; }
                    wi++; continue;
                }
                float gap = first ? 0.f : kCG_ItemGapH;
                if (!first && rowW + gap + w.width > g.max_width) {
                    rowMeasures.push_back({lastWi, rowW}); rowW = 0.f; first = true; gap = 0.f;
                }
                rowW += gap + w.width; lastWi = wi; first = false; wi++;
            }
            if (!first) rowMeasures.push_back({lastWi, rowW});
        }
        float maxNatRowW = 0.f;
        for (auto& r : rowMeasures) maxNatRowW = std::max(maxNatRowW, r.width);

        // ── Pass 2: render, stretching last widget on each shorter row ─────────
        ImGui::SetCursorPos({contentLocalX, contentLocalY});
        ImGui::BeginGroup();

        float rowEndX      = lsX(contentLocalX);
        float maxRowEndX   = lsX(contentLocalX);
        float rowLocalY    = contentLocalY;
        float rowH         = ImGui::GetFrameHeight();
        bool  firstOnRow   = true;
        int   wi = 0, rowMeasIdx = 0;

        for (const auto& w : g.widgets) {
            if (!w.render) {
                if (!firstOnRow) {
                    maxRowEndX  = std::max(maxRowEndX, rowEndX);
                    rowLocalY  += rowH + kCG_ItemGapV;
                    rowH        = ImGui::GetFrameHeight();
                    rowEndX     = lsX(contentLocalX);
                    firstOnRow  = true;
                    rowMeasIdx++;
                }
                wi++; continue;
            }

            float gap = firstOnRow ? 0.f : kCG_ItemGapH;
            if (!firstOnRow && rowEndX + gap + w.width > contentMaxX) {
                maxRowEndX  = std::max(maxRowEndX, rowEndX);
                rowLocalY  += rowH + kCG_ItemGapV;
                rowH        = ImGui::GetFrameHeight();
                rowEndX     = lsX(contentLocalX);
                firstOnRow  = true;
                rowMeasIdx++;
            }

            if (lsY(rowLocalY) >= contentMaxY) { wi++; break; }

            // Stretch last widget on shorter rows to match the widest row
            if (rowMeasIdx < (int)rowMeasures.size() &&
                wi == rowMeasures[rowMeasIdx].lastWi) {
                float extra = maxNatRowW - rowMeasures[rowMeasIdx].width;
                if (extra > 0.5f) kCG_StretchExtra = extra;
            }

            if (firstOnRow)
                ImGui::SetCursorPos({contentLocalX, rowLocalY});
            else
                ImGui::SameLine(0, kCG_ItemGapH);

            w.render();

            float iH = ImGui::GetItemRectSize().y;
            rowH       = std::max(rowH, iH > 0.f ? iH : ImGui::GetFrameHeight());
            rowEndX    = ImGui::GetItemRectMax().x;
            firstOnRow = false;
            wi++;
        }
        maxRowEndX = std::max(maxRowEndX, rowEndX);

        ImGui::EndGroup();

        // ── Fieldset border — content-fitted width, variable height ───────────
        ImVec2 iMin = ImGui::GetItemRectMin();
        ImVec2 iMax = ImGui::GetItemRectMax();

        float actualContentW = maxRowEndX - lsX(contentLocalX);
        float actualGroupW   = actualContentW + kCG_PadX + kCG_PadRight;

        ImVec2 bMin = {iMin.x - kCG_PadX,       lsY(curY + labelMid)};
        ImVec2 bMax = {bMin.x + actualGroupW,    iMax.y + kCG_PadBot};
        float  R    = kCG_CornerR;

        float gapS = bMin.x + 6.f;
        float gapE = gapS + (labelW > 0.f ? labelW + 4.f : 0.f);

        // Gradient border: full opacity at top-left, 10% at bottom-right
        const float W = bMax.x - bMin.x;
        const float H = bMax.y - bMin.y;
        auto colAt2D = [&](float x, float y) -> ImU32 {
            float tx = std::max(0.f, std::min(1.f, (x - bMin.x) / W));
            float ty = std::max(0.f, std::min(1.f, (y - bMin.y) / H));
            uint8_t a = (uint8_t)(155.f * (1.f - tx * 0.999f) * (1.f - ty * 0.999f));
            return IM_COL32(62, 62, 95, a);
        };
        auto hLine = [&](float x0, float x1, float y) {
            ImU32 c0 = colAt2D(x0, y), c1 = colAt2D(x1, y);
            dl->AddRectFilledMultiColor({x0, y}, {x1, y + 1.f}, c0, c1, c1, c0);
        };
        auto vLine = [&](float x, float y0, float y1) {
            ImU32 cT = colAt2D(x, y0), cB = colAt2D(x, y1);
            dl->AddRectFilledMultiColor({x, y0}, {x + 1.f, y1}, cT, cT, cB, cB);
        };
        ImU32 cTL = colAt2D(bMin.x, bMin.y), cTR = colAt2D(bMax.x, bMin.y);
        ImU32 cBL = colAt2D(bMin.x, bMax.y), cBR = colAt2D(bMax.x, bMax.y);

        hLine(bMin.x + R, gapS,       bMin.y);
        hLine(gapE,       bMax.x - R, bMin.y);
        hLine(bMin.x + R, bMax.x - R, bMax.y);
        vLine(bMin.x, bMin.y + R, bMax.y - R);
        vLine(bMax.x, bMin.y + R, bMax.y - R);
        // Corners
        dl->AddLine({bMin.x,     bMin.y + R}, {bMin.x + R, bMin.y  }, cTL, 1.f);
        dl->AddLine({bMax.x - R, bMin.y    }, {bMax.x,     bMin.y + R}, cTR, 1.f);
        dl->AddLine({bMin.x + R, bMax.y    }, {bMin.x,     bMax.y - R}, cBL, 1.f);
        dl->AddLine({bMax.x - R, bMax.y    }, {bMax.x,     bMax.y - R}, cBR, 1.f);

        if (labelW > 0.f) {
            float ly = bMin.y - labelMid;
            dl->AddText(g.labelFont,
                        g.labelFont ? g.labelFont->LegacySize : ImGui::GetFontSize(),
                        {gapS + 2.f, ly}, kCG_LabelCol, g.label);
        }

        curX   += actualGroupW + kCG_GapH;
        rowMaxH = std::max(rowMaxH, bMax.y - lsY(curY));
    }
    return (curY - localY) + rowMaxH;
}
