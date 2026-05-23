#include "SidebarView.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>

// ─── Node ID scheme ───────────────────────────────────────────────────────────
//   0        = song root group  (unused in current tree, reserved)
//   1        = Instruments group
//   2        = Samples group
//   3        = Patterns group
//   1000+i   = instrument leaf i  (0-based)
//   2000+i   = sample leaf i
//   3000+i   = pattern leaf i
//  10000+    = reserved for future sound banks
//  20000+    = reserved for future sound banks

static constexpr int kIdSongRoot    = 0;
static constexpr int kIdInstrGroup  = 1;
static constexpr int kIdSampleGroup = 2;
static constexpr int kIdPatGroup    = 3;
static constexpr int kBaseInstr     = 1000;
static constexpr int kBaseSample    = 2000;
static constexpr int kBasePat       = 3000;

// ─── resetState ──────────────────────────────────────────────────────────────

void SidebarView::resetState()
{
    m_cursorId = -1;
    // Keep m_expanded so the user's open/close state survives module reloads.
    // Caller can clear it separately if desired.
}

// ─── isExpanded ──────────────────────────────────────────────────────────────

bool SidebarView::isExpanded(int id) const
{
    auto it = m_expanded.find(id);
    if (it == m_expanded.end()) return true;   // default: open
    return it->second;
}

// ─── buildNodes ──────────────────────────────────────────────────────────────

void SidebarView::buildNodes(std::vector<Node>& out, const SidebarData& d) const
{
    out.clear();

    // ── Song root ─────────────────────────────────────────────────────────────
    // Leading spaces reserve room for the RenderArrow drawn in render().
    char rootLabel[256];
    snprintf(rootLabel, sizeof(rootLabel), "   %s",
             d.songTitle.empty() ? "(untitled)" : d.songTitle.c_str());
    out.push_back({kIdSongRoot, 0, true, true, rootLabel, -1, -1});
    if (!isExpanded(kIdSongRoot)) return;

    // ── Instruments ──────────────────────────────────────────────────────────
    if (!d.instrNames.empty()) {
        char grpLabel[64];
        snprintf(grpLabel, sizeof(grpLabel), "     Instruments (%d)",
                 (int)d.instrNames.size());
        out.push_back({kIdInstrGroup, 1, true, true, grpLabel, -1, -1});

        if (isExpanded(kIdInstrGroup)) {
            for (int i = 0; i < (int)d.instrNames.size(); ++i) {
                char row[256];
                snprintf(row, sizeof(row), "      %02d  %s", i + 1,
                         d.instrNames[i].empty() ? "---" : d.instrNames[i].c_str());
                out.push_back({kBaseInstr + i, 2, false, true, row, 3, i});
            }
        }
    }

    // ── Samples ──────────────────────────────────────────────────────────────
    {
        char grpLabel[64];
        snprintf(grpLabel, sizeof(grpLabel), "     Samples (%d)",
                 (int)d.sampleNames.size());
        out.push_back({kIdSampleGroup, 1, true, true, grpLabel, -1, -1});

        if (isExpanded(kIdSampleGroup)) {
            for (int i = 0; i < (int)d.sampleNames.size(); ++i) {
                char row[256];
                snprintf(row, sizeof(row), "      %02d  %s", i + 1,
                         d.sampleNames[i].empty() ? "---" : d.sampleNames[i].c_str());
                out.push_back({kBaseSample + i, 2, false, true, row, 2, i});
            }
        }
    }

    // ── Patterns ─────────────────────────────────────────────────────────────
    {
        char grpLabel[64];
        snprintf(grpLabel, sizeof(grpLabel), "     Patterns (%d)",
                 (int)d.patNames.size());
        out.push_back({kIdPatGroup, 1, true, true, grpLabel, -1, -1});

        if (isExpanded(kIdPatGroup)) {
            for (int i = 0; i < (int)d.patNames.size(); ++i) {
                bool used = d.usedPats.count(i) > 0;
                char row[256];
                snprintf(row, sizeof(row), "    %02d  %s", i,
                         d.patNames[i].empty() ? "---" : d.patNames[i].c_str());
                out.push_back({kBasePat + i, 2, false, used, row, 1, i});
            }
        }
    }
}

// ─── render ──────────────────────────────────────────────────────────────────

void SidebarView::render(float x, float y, float w, float h,
                         bool hasModule, const SidebarData& data,
                         float topPad)
{
    // ── Window style ──────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 20, 40, 255));
    // Suppress all built-in selection/hover/nav visuals — we draw our own.
    ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(8.f, 4.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(4.f, 2.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);
    ImGui::Begin("##sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar      |
        ImGuiWindowFlags_NoResize        |
        ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar     |
        ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    if (!hasModule) {
        ImGui::TextDisabled("No module loaded");
        ImGui::End();
        return;
    }

    if (topPad > 0.f)
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + topPad);

    // ── Scrollable child ──────────────────────────────────────────────────────
    {
        const float rp     = ImGui::GetStyle().WindowPadding.x;
        const float sw     = ImGui::GetStyle().ScrollbarSize;
        const float childW = ImGui::GetContentRegionAvail().x + rp;
        ImGui::SetNextWindowContentSize({childW - sw, 0.f});
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          IM_COL32( 12,  12,  22, 255));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        IM_COL32( 40,  85, 145, 200));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32( 55, 112, 175, 230));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  IM_COL32( 66, 130, 205, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##sidebar_scroll", {childW, 0.f}, false, 0);
        ImGui::PopStyleVar(1);
    }

    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // ── Build visible node list ───────────────────────────────────────────────
    std::vector<Node> nodes;
    buildNodes(nodes, data);

    // Find cursor index in current node list
    int cursorIdx = -1;
    for (int i = 0; i < (int)nodes.size(); ++i) {
        if (nodes[i].id == m_cursorId) { cursorIdx = i; break; }
    }
    // If cursor node disappeared (e.g. group collapsed), clear cursor
    if (cursorIdx < 0) m_cursorId = -1;

    // ── Keyboard input ────────────────────────────────────────────────────────
    bool scrollToCursor = false;

    if (focused && !nodes.empty()) {
        // Ensure cursor is on some node
        if (cursorIdx < 0) {
            cursorIdx  = 0;
            m_cursorId = nodes[0].id;
            scrollToCursor = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
            if (cursorIdx > 0) {
                --cursorIdx;
                m_cursorId     = nodes[cursorIdx].id;
                scrollToCursor = true;
            }
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
            if (cursorIdx < (int)nodes.size() - 1) {
                ++cursorIdx;
                m_cursorId     = nodes[cursorIdx].id;
                scrollToCursor = true;
            }
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
            const Node& cur = nodes[cursorIdx];
            if (cur.isGroup && isExpanded(cur.id)) {
                // Collapse group
                m_expanded[cur.id] = false;
            } else {
                // Move cursor to parent group (depth 0 group) if we're on a leaf
                if (cur.depth > 0) {
                    // Walk backwards to find the nearest group at lower depth
                    for (int k = cursorIdx - 1; k >= 0; --k) {
                        if (nodes[k].isGroup && nodes[k].depth < cur.depth) {
                            cursorIdx      = k;
                            m_cursorId     = nodes[k].id;
                            scrollToCursor = true;
                            break;
                        }
                    }
                }
            }
            // Rebuild after expand/collapse change, re-find cursor
            buildNodes(nodes, data);
            cursorIdx = -1;
            for (int i = 0; i < (int)nodes.size(); ++i) {
                if (nodes[i].id == m_cursorId) { cursorIdx = i; break; }
            }
            if (cursorIdx < 0 && !nodes.empty()) {
                cursorIdx  = 0;
                m_cursorId = nodes[0].id;
            }
            scrollToCursor = true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
            const Node& cur = nodes[cursorIdx];
            if (cur.isGroup && !isExpanded(cur.id)) {
                m_expanded[cur.id] = true;
            }
            // Rebuild after expand/collapse change, re-find cursor
            buildNodes(nodes, data);
            cursorIdx = -1;
            for (int i = 0; i < (int)nodes.size(); ++i) {
                if (nodes[i].id == m_cursorId) { cursorIdx = i; break; }
            }
            if (cursorIdx < 0 && !nodes.empty()) {
                cursorIdx  = 0;
                m_cursorId = nodes[0].id;
            }
            scrollToCursor = true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
            const Node& cur = nodes[cursorIdx];
            if (!cur.isGroup && cur.enabled && onLeafActivated) {
                onLeafActivated(cur.tab, cur.idx);
            }
        }
    }

    // ── Render nodes ──────────────────────────────────────────────────────────
    // All ImGui selection visuals are transparent (pushed globally above).
    // We draw our own full-width highlight rect before each Selectable.

    ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight,  IM_COL32(0, 0, 0, 0));

    const ImU32 hlFocused = IM_COL32(50,  95, 165, 255);
    const ImU32 hlDim     = IM_COL32(45,  45,  75, 255);

    for (int i = 0; i < (int)nodes.size(); ++i) {
        const Node& n = nodes[i];

        ImGui::PushID(n.id);

        const bool isCursor = (i == cursorIdx);

        // Draw highlight rect spanning the full window width
        if (isCursor) {
            ImVec2 p       = ImGui::GetCursorScreenPos();
            float  rowH    = ImGui::GetTextLineHeightWithSpacing();
            float  winLeft = ImGui::GetWindowPos().x;
            float  winRight = winLeft + ImGui::GetWindowWidth();
            ImU32  hlColor = focused ? hlFocused : hlDim;
            ImGui::GetWindowDrawList()->AddRectFilled(
                {winLeft, p.y}, {winRight, p.y + rowH}, hlColor);
        }

        if (n.isGroup) {
            // Draw ImGui-style arrow — same style as TreeNodeEx (full scale, text color)
            {
                ImVec2 p    = ImGui::GetCursorScreenPos();
                float  sz   = ImGui::GetTextLineHeight();
                float  rowH = ImGui::GetTextLineHeightWithSpacing();
                float  yOff = (rowH - sz * 0.7f) * 0.5f;
                ImGui::RenderArrow(ImGui::GetWindowDrawList(),
                    {p.x + n.depth * sz, p.y + yOff},
                    IM_COL32(180, 200, 230, 220),
                    isExpanded(n.id) ? ImGuiDir_Down : ImGuiDir_Right,
                    0.7f);
            }
            // Group row: clicking toggles expand/collapse
            if (ImGui::Selectable(n.label.c_str(), false, 0, {0.f, 0.f})) {
                m_expanded[n.id] = !isExpanded(n.id);
                m_cursorId = n.id;
                cursorIdx  = i;
            }
        } else {
            // Leaf row
            if (!n.enabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 180, 200, 200));
            }

            ImGuiSelectableFlags selFlags = n.enabled ? 0 : ImGuiSelectableFlags_Disabled;
            if (ImGui::Selectable(n.label.c_str(), false, selFlags, {0.f, 0.f})) {
                m_cursorId = n.id;
                cursorIdx  = i;
                if (n.enabled && onLeafActivated) {
                    onLeafActivated(n.tab, n.idx);
                }
            }

            if (!n.enabled) {
                ImGui::PopStyleColor();
            }
        }

        // Scroll to keep cursor visible after keyboard movement
        if (scrollToCursor && isCursor) {
            ImGui::SetScrollHereY(0.5f);
            scrollToCursor = false;
        }

        ImGui::PopID();
    }

    ImGui::PopStyleColor(3);   // Header, HeaderHovered, NavHighlight

    ImGui::EndChild();
    ImGui::PopStyleColor(4);   // scrollbar colors

    ImGui::End();
}
