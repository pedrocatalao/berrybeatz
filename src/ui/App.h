#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "ModuleState.h"
#include "ModuleController.h"
#include "AudioEngine.h"
#include "TrackerView.h"
#include "PropertiesView.h"
#include "InstrumentView.h"
#include "SampleView.h"
#include "Toolbar.h"
#include "WindowChrome.h"
#include "SettingsView.h"
#include "Settings.h"
#include "MidiEngine.h"
#include "SidebarView.h"
#include "NewModuleDialog.h"
#include "SongDocument.h"
#include "PianoKeyboard.h"
#include <string>
#include <vector>
#include <memory>

struct ImFont;
struct GLFWwindow;

class App {
public:
    App();
    ~App();

    void setFonts(ImFont* mono, ImFont* ui);
    void setGLFWWindow(GLFWwindow* w);
    void openFile(const std::string& path);
    void tickChrome()        { m_chrome.tick(); }
    void toggleFullscreen()  { m_chrome.toggleFullscreen(); }
    void render(int winW, int winH);
    bool isPlaying()   const { return m_transport == Transport::Playing || (m_audio.isRunning() && !m_audio.isPaused()); }
    bool isAnimating() const { return m_kbdAnimT > 0.f && m_kbdAnimT < 1.f; }

private:
    void syncToolbarState();
    void drawSamplesTab();
    void drawInstrumentsTab();
    void drawCommentsTab();
    void drawSongBar(float w, float h);
    void openFileDialog();
    void saveModule();
    void insertPattern();
    void switchToSong(int idx);
    void closeSong(int idx);
    void deactivateAllSongs();   // stop audio, clear views — called when last song closes
    void activateViews();        // wire views to active song after switchToSong
    void startPlayback();
    void stopPlayback();
    void togglePlayPause();
    void playFromStart();
    void seekTo(float frac);
    void dispatchMidiCC(int cc, int value);
    void wireNoteRT();

    ModuleState      m_state;
    ModuleController m_controller;
    AudioEngine      m_audio;
    TrackerView      m_tracker;
    PropertiesView   m_generalView;
    InstrumentView   m_instrView;
    SampleView       m_sampleView;

    ToolbarState     m_toolbarState;
    WindowChrome     m_chrome;
    SettingsView     m_settingsView;
    NewModuleDialog  m_newModuleDialog;
    Settings         m_settings;
    MidiEngine       m_midi;
    SidebarView      m_sidebar;

    GLFWwindow* m_glfwWindow {nullptr};
    ImFont*     m_fontMono   {nullptr};
    ImFont*     m_fontUI     {nullptr};

    // Song management
    std::vector<std::unique_ptr<SongDocument>> m_songs;
    int   m_activeSongIdx {-1};
    bool  m_hasModule     {false};

    Transport m_transport     {Transport::Stopped};
    bool      m_inPatternPlay {false};

    PianoKeyboardState m_kbState;
    float              m_kbdAnimT {0.f};   // 0=hidden, 1=fully shown

    std::atomic<int> m_activeTab       {0};
    // TODO: single-slot — drops all but the last note if two MIDI notes arrive in one frame. Fine for
    // monophonic step entry; replace with a lock-free ring if chord recording is ever needed.
    std::atomic<int> m_midiTrackerNote {-1};
    int              m_pendingTab {-1};
    bool             m_tabsInited {false};

    float m_sidebarW {260.f};

    GLuint m_logoTex {0};
    float  m_logoW   {0.f};
    float  m_logoH   {0.f};


    static constexpr float kToolbarH  = 79.f;
    static constexpr float kTabBarH   = 26.f;
    static constexpr float kSongBarH  = 26.f;
    static constexpr float kSplitterW =  6.f;
};
