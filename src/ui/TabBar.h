#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "Theme.h"

// ---------------------------------------------------------------------------
// Styled tab bar helpers — encapsulate BerryBeatz's tab push/pop boilerplate
// and per-tab active/inactive text colouring. Follows ImGui's own begin/end
// idiom so call sites stay immediate-mode idiomatic:
//
//   if (BeginStyledTabBar("##id")) {
//       if (StyledTabItem("Label")) { /* content */ ImGui::EndTabItem(); }
//       EndStyledTabBar();
//   }
// ---------------------------------------------------------------------------

inline bool BeginStyledTabBar(const char* id)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,     kTabFramePad);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, kTabInnerSpacing);
    ImGui::PushStyleColor(ImGuiCol_Tab,        kClrTabBg);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, kClrTabHover);
    bool open = ImGui::BeginTabBar(id);
    if (!open) {
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
    return open;
}

// Must only be called when BeginStyledTabBar returned true.
inline void EndStyledTabBar()
{
    ImGui::EndTabBar();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// Drop-in for ImGui::BeginTabItem with automatic active/inactive text colour.
// Call ImGui::EndTabItem() if this returns true.
inline bool StyledTabItem(const char* label, bool* p_open = nullptr,
                          ImGuiTabItemFlags flags = 0)
{
    ImGuiTabBar* bar = ImGui::GetCurrentTabBar();
    bool selected    = bar && (bar->SelectedTabId == ImGui::GetID(label));
    ImGui::PushStyleColor(ImGuiCol_Text, selected ? kClrTextBright : kClrTextDim);
    bool visible = ImGui::BeginTabItem(label, p_open, flags);
    ImGui::PopStyleColor();
    return visible;
}
