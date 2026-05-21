#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "ModuleState.h"
#include "AudioEngine.h"
#include "TrackerView.h"
#include "GeneralView.h"
#include "InstrumentView.h"
#include "SampleView.h"
#include "Toolbar.h"
#include "WindowChrome.h"
#include "SettingsView.h"
#include "Settings.h"
#include <string>

struct ImFont;
struct GLFWwindow;

class App {
public:
    App();
    ~App();

    void setFonts(ImFont* mono, ImFont* ui);
    void setGLFWWindow(GLFWwindow* w);
    void openFile(const std::string& path);
    void tickChrome() { m_chrome.tick(); }  // call before ImGui::NewFrame each frame
    void render(int winW, int winH);

private:
    void syncToolbarState();
    void drawSidebar(float x, float y, float w, float h);
    void drawSamplesTab();
    void drawInstrumentsTab();
    void drawCommentsTab();
    void openFileDialog();
    void saveModule();
    void insertPattern();
    void clearModule();
    void startPlayback();
    void stopPlayback();
    void togglePlayPause();
    void playFromStart();
    void seekTo(float frac);

    ModuleState    m_state;
    AudioEngine    m_audio;
    TrackerView    m_tracker;
    GeneralView    m_generalView;
    InstrumentView m_instrView;
    SampleView     m_sampleView;

    ToolbarState   m_toolbarState;
    WindowChrome   m_chrome;
    SettingsView   m_settingsView;
    Settings       m_settings;

    GLFWwindow* m_glfwWindow {nullptr};
    ImFont*     m_fontMono   {nullptr};
    ImFont*     m_fontUI     {nullptr};

    int   m_activeTab  {0};
    bool  m_tabsInited {false};
    bool  m_hasModule  {false};

    float m_sidebarW {260.f};

    GLuint m_logoTex {0};
    float  m_logoW   {0.f};
    float  m_logoH   {0.f};

    GLuint m_speakerConeTex {0};
    float  m_speakerConeW   {0.f};
    float  m_speakerConeH   {0.f};

    static constexpr float kToolbarH  = 78.f;
    static constexpr float kTabBarH   = 26.f;
    static constexpr float kSplitterW =  6.f;  // hit area; line drawn at 1px centre

    std::string m_loadedPath;
};
