#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "ModuleState.h"
#include "AudioEngine.h"
#include "TrackerView.h"
#include "Toolbar.h"
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
    void render(int winW, int winH);

private:
    void syncToolbarState();
    void drawSidebar(float x, float y, float w, float h);
    void drawGeneralTab();
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
    void handleEdgeResize(int winW, int winH);

    ModuleState   m_state;
    AudioEngine   m_audio;
    TrackerView   m_tracker;

    ToolbarState    m_toolbarState;

    GLFWwindow* m_glfwWindow {nullptr};
    ImFont*     m_fontMono   {nullptr};
    ImFont*     m_fontUI     {nullptr};

    int   m_activeTab  {0};
    bool  m_tabsInited {false};
    bool  m_hasModule  {false};

    float m_sidebarW {200.f};

    bool m_isMaximized  {false};
    int  m_restoreX {0}, m_restoreY {0}, m_restoreW {1280}, m_restoreH {800};

    bool m_isFullscreen {false};
    int  m_fsRestoreX {0}, m_fsRestoreY {0}, m_fsRestoreW {1280}, m_fsRestoreH {800};

    GLuint m_logoTex {0};
    float  m_logoW   {0.f};
    float  m_logoH   {0.f};

    enum class ResizeEdge { None, Right, Bottom, BottomRight };
    ResizeEdge m_resizeEdge {ResizeEdge::None};
    int m_rsMouseX {0}, m_rsMouseY {0};
    int m_rsWinW   {0}, m_rsWinH   {0};

    static constexpr float kToolbarH  = 78.f;
    static constexpr float kTabBarH   = 26.f;
    static constexpr float kSplitterW =  6.f;  // hit area; line drawn at 1px centre

    std::string m_loadedPath;
};
