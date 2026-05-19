#pragma once
#include "ModuleState.h"
#include "AudioEngine.h"
#include "TrackerView.h"
#include "Toolbar.h"
#include <string>
#include <vector>
#include <filesystem>

struct ImFont;

class App {
public:
    App();
    ~App();

    void setFonts(ImFont* mono, ImFont* ui);
    void openFile(const std::string& path);
    void render(int winW, int winH);

private:
    // Per-frame state sync
    void syncToolbarState();

    // Sidebar / browser
    void drawSidebar(float x, float y, float w, float h);
    void browserNavigate(const std::filesystem::path& path);

    // Modal
    void drawKnobEditModal();

    // Tab content
    void drawGeneralTab();
    void drawSamplesTab();
    void drawInstrumentsTab();
    void drawCommentsTab();

    // Operations
    void openFileDialog();
    void clearModule();
    void startPlayback();
    void stopPlayback();
    void togglePlayPause();
    void playFromStart();
    void seekTo(float frac);

    ModuleState   m_state;
    AudioEngine   m_audio;
    TrackerView   m_tracker;

    ToolbarState    m_toolbarState;
    KnobEditRequest m_knobEdit;
    int             m_editValue {0};

    ImFont* m_fontMono {nullptr};
    ImFont* m_fontUI   {nullptr};

    int   m_activeTab  {0};
    bool  m_tabsInited {false};
    bool  m_hasModule  {false};

    // Browser state
    std::filesystem::path                        m_browserPath;
    std::vector<std::filesystem::directory_entry> m_browserEntries;
    float m_sidebarW {220.f};

    static constexpr float kToolbarH  = 60.f;
    static constexpr float kTabBarH   = 22.f;
    static constexpr float kSplitterW =  4.f;

    std::string m_loadedPath;
};
