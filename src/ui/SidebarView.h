#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SidebarData {
    std::string              songTitle;
    std::vector<std::string> instrNames;
    std::vector<std::string> sampleNames;
    std::vector<std::string> patNames;
    std::unordered_set<int>  usedPats;
};

class SidebarView {
public:
    // Called when user activates a leaf (click or Enter).
    // tab: 1=tracker/patterns, 2=samples, 3=instruments; idx: 0-based item index.
    std::function<void(int tab, int idx)> onLeafActivated;

    void render(float x, float y, float w, float h,
                bool hasModule, const SidebarData& data);

    // Call when a new module is loaded/cleared so stale cursor state is reset.
    void resetState();

private:
    struct Node {
        int  id;
        int  depth;
        bool isGroup;
        bool enabled;   // false = dimmed + non-activatable (unused patterns)
        std::string label;
        int  tab{-1};   // leaf: tab to switch to
        int  idx{-1};   // leaf: 0-based item index
    };

    void buildNodes(std::vector<Node>& out, const SidebarData& d) const;
    bool isExpanded(int id) const;

    std::unordered_map<int, bool> m_expanded;
    int m_cursorId{-1};
};
