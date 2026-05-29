#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SoundLib {
    std::string name;   // display label e.g. "GarageBand Loops"
    std::string path;   // absolute root directory path
};

struct SidebarData {
    std::string              songTitle;
    std::vector<std::string> instrNames;
    std::vector<std::string> sampleNames;
    std::vector<std::string> patNames;
    std::unordered_set<int>  usedPats;
    std::vector<SoundLib>    soundLibs;   // file browser roots
};

class SidebarView {
public:
    // Called when user activates a module leaf (click or Enter).
    // tab: 1=tracker/patterns, 2=samples, 3=instruments; idx: 0-based item index.
    std::function<void(int tab, int idx)>          onLeafActivated;

    // Called when user clicks a file in the sound library browser.
    std::function<void(const std::string& path)>   onFilePreview;

    // Called when user double-clicks a file (import into next free sample slot).
    std::function<void(const std::string& path)>   onFileImport;

    void render(float x, float y, float w, float h,
                bool hasModule, const SidebarData& data,
                float topPad = 0.f);

    // Call when a new module is loaded/cleared so stale cursor state is reset.
    void resetState();

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
        std::string path;   // non-empty for file-browser nodes
        bool isFile{false}; // true = audio file leaf, false = dir or module node
    };

    struct DirEntry {
        std::string name;
        std::string path;
        bool        isDir;
    };

    void buildNodes(std::vector<Node>& out, const SidebarData& d);
    bool isExpanded(int id) const;
    const std::vector<DirEntry>& dirEntries(const std::string& path);
    int  pathId(const std::string& path);

    std::unordered_map<int, bool>                m_expanded;
    std::unordered_map<std::string, int>         m_pathToId;
    std::unordered_map<int, std::string>         m_idToPath;
    std::unordered_map<std::string, std::vector<DirEntry>> m_dirCache;
    int  m_nextId   {10000};
    int  m_cursorId {-1};
};
