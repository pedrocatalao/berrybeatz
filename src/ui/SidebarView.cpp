#include "SidebarView.h"
#include "Theme.h"
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

// ─── collapseAll ─────────────────────────────────────────────────────────────

void SidebarView::collapseAll(int s)
{
    int base = s * 10000;
    m_expanded[base + kIdSongRoot]    = false;
    m_expanded[base + kIdInstrGroup]  = false;
    m_expanded[base + kIdSampleGroup] = false;
    m_expanded[base + kIdPatGroup]    = false;
}

// ─── isExpanded ──────────────────────────────────────────────────────────────

bool SidebarView::isExpanded(int id) const
{
    auto it = m_expanded.find(id);
    if (it == m_expanded.end()) return true;   // default: open
    return it->second;
}

// ─── buildNodes ──────────────────────────────────────────────────────────────

void SidebarView::buildNodes(std::vector<Node>& out, const std::vector<SidebarData>& songs) const
{
    out.clear();
    for (const SidebarData& d : songs) {
        const int s    = d.songIdx;
        const int base = s * 10000;

        // ── Song root ─────────────────────────────────────────────────────────
        out.push_back({base + kIdSongRoot, 0, true, true,
                       d.songTitle.empty() ? "(untitled)" : d.songTitle,
                       -1, -1, -1, s});
        if (!isExpanded(base + kIdSongRoot)) continue;

        // ── Instruments ──────────────────────────────────────────────────────
        if (!d.instrNames.empty()) {
            char grp[64];
            snprintf(grp, sizeof(grp), "Instruments (%d)", (int)d.instrNames.size());
            out.push_back({base + kIdInstrGroup, 1, true, true, grp, -1, -1, -1, s});

            if (isExpanded(base + kIdInstrGroup)) {
                for (int i = 0; i < (int)d.instrNames.size(); ++i)
                    out.push_back({base + kBaseInstr + i, 2, false, true,
                                   d.instrNames[i].empty() ? "---" : d.instrNames[i],
                                   i + 1, 3, i, s});
            }
        }

        // ── Samples ──────────────────────────────────────────────────────────
        {
            char grp[64];
            snprintf(grp, sizeof(grp), "Samples (%d)", (int)d.sampleNames.size());
            out.push_back({base + kIdSampleGroup, 1, true, true, grp, -1, -1, -1, s});

            if (isExpanded(base + kIdSampleGroup)) {
                for (int i = 0; i < (int)d.sampleNames.size(); ++i)
                    out.push_back({base + kBaseSample + i, 2, false, true,
                                   d.sampleNames[i].empty() ? "---" : d.sampleNames[i],
                                   i + 1, 2, i, s});
            }
        }

        // ── Patterns ─────────────────────────────────────────────────────────
        {
            char grp[64];
            snprintf(grp, sizeof(grp), "Patterns (%d)", (int)d.patNames.size());
            out.push_back({base + kIdPatGroup, 1, true, true, grp, -1, -1, -1, s});

            if (isExpanded(base + kIdPatGroup)) {
                for (int i = 0; i < (int)d.patNames.size(); ++i) {
                    bool used = d.usedPats.count(i) > 0;
                    out.push_back({base + kBasePat + i, 2, false, used,
                                   d.patNames[i].empty() ? "---" : d.patNames[i],
                                   i, 1, i, s});
                }
            }
        }
    }
}

// ─── render ──────────────────────────────────────────────────────────────────

void SidebarView::render(float x, float y, float w, float h,
                         bool hasModule, const std::vector<SidebarData>& songs,
                         float topPad)
{
    // ── Window style ──────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
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
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          kClrScrollBg);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        kClrScrollGrab);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, kClrScrollHover);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  kClrScrollActive);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##sidebar_scroll", {childW, 0.f}, false, 0);
        ImGui::PopStyleVar(1);
    }

    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // ── Build visible node list ───────────────────────────────────────────────
    std::vector<Node> nodes;
    buildNodes(nodes, songs);

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
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
            // Initialize cursor on first keyboard navigation
            if (cursorIdx < 0) { cursorIdx = 0; m_cursorId = nodes[0].id; }
            if (cursorIdx > 0) {
                --cursorIdx;
                m_cursorId     = nodes[cursorIdx].id;
                scrollToCursor = true;
            }
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
            if (cursorIdx < 0) { cursorIdx = 0; m_cursorId = nodes[0].id; scrollToCursor = true; }
            else if (cursorIdx < (int)nodes.size() - 1) {
                ++cursorIdx;
                m_cursorId     = nodes[cursorIdx].id;
                scrollToCursor = true;
            }
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) && cursorIdx >= 0) {
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
            buildNodes(nodes, songs);
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
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) && cursorIdx >= 0) {
            const Node& cur = nodes[cursorIdx];
            if (cur.isGroup && !isExpanded(cur.id)) {
                m_expanded[cur.id] = true;
            }
            // Rebuild after expand/collapse change, re-find cursor
            buildNodes(nodes, songs);
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
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && cursorIdx >= 0) {
            const Node& cur = nodes[cursorIdx];
            if (!cur.isGroup && cur.enabled && onLeafActivated) {
                onLeafActivated(cur.songIdx, cur.tab, cur.idx);
            }
        }
    }

    // ── Render nodes ──────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight,  IM_COL32(0, 0, 0, 0));

    const ImU32 kHlFocused = IM_COL32( 50,  95, 165, 255);
    const ImU32 kHlDim     = IM_COL32( 45,  45,  75, 255);
    const ImU32 kArrowCol  = IM_COL32(180, 200, 230, 220);
    const ImU32 kTextNorm  = IM_COL32(208, 208, 224, 255);
    const ImU32 kTextDim   = IM_COL32( 80,  80,  90, 255);
    const ImU32 kNumYellow = IM_COL32(224, 224,  64, 255);  // same as kCellTextNormal

    const float sz        = ImGui::GetTextLineHeight();
    const float rowH      = ImGui::GetTextLineHeightWithSpacing();
    const float kIndent   = sz + 2.f;   // one level of indent = arrow width + small gap
    const float numW      = ImGui::CalcTextSize("00").x + 6.f;  // fixed number column

    for (int i = 0; i < (int)nodes.size(); ++i) {
        const Node& n = nodes[i];
        ImGui::PushID(n.id);

        const bool isCursor = (i == cursorIdx);
        ImDrawList* dl   = ImGui::GetWindowDrawList();
        ImVec2 rowPos    = ImGui::GetCursorScreenPos();
        float  winLeft   = ImGui::GetWindowPos().x;
        float  winRight  = winLeft + ImGui::GetWindowWidth();

        // Full-width highlight rect
        if (isCursor) {
            ImU32 hlColor = focused ? kHlFocused : kHlDim;
            dl->AddRectFilled({winLeft, rowPos.y}, {winRight, rowPos.y + rowH}, hlColor);
        }

        // Invisible hit area (transparent selectable, explicit row height)
        float hitW = ImGui::GetContentRegionAvail().x;
        ImGuiSelectableFlags selFlags = (!n.isGroup && !n.enabled)
                                        ? ImGuiSelectableFlags_Disabled : 0;
        bool clicked = ImGui::Selectable("##node", false, selFlags, {hitW, rowH});

        // Content drawn on the draw list — independent of ImGui cursor position
        float cx   = rowPos.x + n.depth * kIndent;   // content start X for this depth
        float textY = rowPos.y + (rowH - sz) * 0.5f; // vertically centred

        if (n.isGroup) {
            float arrowY = rowPos.y + (rowH - sz * 0.7f) * 0.5f;
            ImGui::RenderArrow(dl, {cx, arrowY}, kArrowCol,
                isExpanded(n.id) ? ImGuiDir_Down : ImGuiDir_Right, 0.7f);
            dl->AddText({cx + sz + 2.f, textY}, kTextNorm, n.name.c_str());
        } else {
            if (n.displayNum >= 0) {
                char numBuf[8];
                snprintf(numBuf, sizeof(numBuf), "%02d", n.displayNum);
                float numX = cx + numW - ImGui::CalcTextSize(numBuf).x - 3.f;
                dl->AddText({numX, textY}, n.enabled ? kNumYellow : kTextDim, numBuf);
            }
            dl->AddText({cx + numW, textY}, n.enabled ? kTextNorm : kTextDim, n.name.c_str());
        }

        if (clicked) {
            m_cursorId = n.id;
            cursorIdx  = i;
            if (n.isGroup) {
                m_expanded[n.id] = !isExpanded(n.id);
            } else if (n.enabled && onLeafActivated) {
                onLeafActivated(n.songIdx, n.tab, n.idx);
            }
        }

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
