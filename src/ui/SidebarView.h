#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SidebarData {
    int                      songIdx{0};
    bool                     isActive{false};
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
    std::function<void(int songIdx, int tab, int idx)> onLeafActivated;

    void render(float x, float y, float w, float h,
                bool hasModule, const std::vector<SidebarData>& songs,
                float topPad = 0.f);

    // Call when a new module is loaded/cleared so stale cursor state is reset.
    void resetState();
    // Collapse all group nodes for a given song (call on module load).
    void collapseAll(int songIdx);

private:
    struct Node {
        int  id;
        int  depth;
        bool isGroup;
        bool enabled;
        std::string name;
        int  displayNum{-1};
        int  tab{-1};
        int  idx{-1};
        int  songIdx{0};
    };

    void buildNodes(std::vector<Node>& out, const std::vector<SidebarData>& songs) const;
    bool isExpanded(int id) const;

    std::unordered_map<int, bool> m_expanded;
    int m_cursorId{-1};
};
