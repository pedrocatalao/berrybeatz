#pragma once
#include "imgui.h"
#include "Theme.h"

// CustomTabBar — SwitchButton-styled top tab bar, drop-in for BeginStyledTabBar.
//
// Usage:
//   if (BeginCustomTabBar("##id")) {          // forceActive=-1 = use stored state
//       if (CustomTabItem("Label")) {
//           /* content */
//           EndCustomTabItem();               // no-op — keeps call-site symmetry
//       }
//       EndCustomTabBar();
//   }
//
// forceActive: pass a 0-based index to programmatically select a tab this frame.

namespace detail {
    inline struct CustomTabBarCtx {
        ImGuiID barId     {0};
        int     tabCount  {0};
        int     activeIdx {0};
    } g_ctb;
}

inline bool BeginCustomTabBar(const char* id, int forceActive = -1)
{
    detail::g_ctb.barId    = ImGui::GetID(id);
    detail::g_ctb.tabCount = 0;
    ImGuiStorage* st       = ImGui::GetStateStorage();
    detail::g_ctb.activeIdx = (forceActive >= 0) ? forceActive
                                                  : st->GetInt(detail::g_ctb.barId, 0);
    ImGui::PushID(id);
    return true;
}

// Returns the active tab index so callers can render content after the full tab row.
inline int EndCustomTabBar()
{
    int active = detail::g_ctb.activeIdx;
    ImGuiStorage* st = ImGui::GetStateStorage();
    st->SetInt(detail::g_ctb.barId, active);
    ImGui::PopID();
    return active;
}

// Returns true every frame while this tab is active (same semantics as BeginTabItem).
inline bool CustomTabItem(const char* label, bool* = nullptr, ImGuiTabItemFlags flags = 0)
{
    int i = detail::g_ctb.tabCount++;
    if (i > 0) ImGui::SameLine(0, 2.f);

    if (flags & ImGuiTabItemFlags_SetSelected)
        detail::g_ctb.activeIdx = i;

    bool selected = (detail::g_ctb.activeIdx == i);

    const float R    = ImGui::GetStyle().TabRounding;
    const float padX = kTabFramePad.x;
    const float tabH = ImGui::GetTextLineHeight() + kTabFramePad.y * 2.f;

    ImVec2 ts   = ImGui::CalcTextSize(label);
    float  tw   = padX * 2.f + ts.x;
    ImVec2 tMin = ImGui::GetCursorScreenPos();
    ImVec2 tMax = {tMin.x + tw, tMin.y + tabH};

    ImGui::InvisibleButton(label, {tw, tabH});
    bool hov  = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    if (ImGui::IsItemClicked()) detail::g_ctb.activeIdx = i;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 selCol  = kClrTabSelected;
    ImU32 bodyCol = selected    ? selCol
                  : hov || held ? kClrTabHover
                                : kClrTabBg;
    dl->AddRectFilled(tMin, tMax, bodyCol, R, ImDrawFlags_RoundCornersTop);

    float bvR = tMax.x - 1.f;
    float bvB = tMax.y - 1.f;
    if (!held) {
        dl->AddLine({tMin.x + R,    tMin.y + 0.5f}, {bvR - R, tMin.y + 0.5f}, IM_COL32(255,255,255,55));
        dl->AddLine({tMin.x + 0.5f, tMin.y + R   }, {tMin.x + 0.5f, bvB    }, IM_COL32(255,255,255,40));
        dl->AddLine({tMin.x,        bvB           }, {bvR,     bvB           }, IM_COL32(0,0,0,120), 2.f);
        dl->AddLine({bvR,           tMin.y        }, {bvR,     bvB           }, IM_COL32(0,0,0,100));
    } else {
        dl->AddLine({tMin.x + R,    tMin.y + 0.5f}, {bvR - R, tMin.y + 0.5f}, IM_COL32(0,0,0,100));
        dl->AddLine({tMin.x + 0.5f, tMin.y + R   }, {tMin.x + 0.5f, bvB    }, IM_COL32(0,0,0,80));
        dl->AddLine({tMin.x,        bvB           }, {bvR,     bvB           }, IM_COL32(255,255,255,35), 2.f);
        dl->AddLine({bvR,           tMin.y        }, {bvR,     bvB           }, IM_COL32(255,255,255,25));
    }

    dl->AddText({tMin.x + padX, tMin.y + (tabH - ts.y) * 0.5f},
                selected ? kClrTextBright : kClrTextDim, label);

    return selected;
}

inline void EndCustomTabItem() {} // no-op — API symmetry with BeginTabItem
