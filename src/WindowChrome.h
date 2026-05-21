#pragma once

struct GLFWwindow;

class WindowChrome {
public:
    void setWindow(GLFWwindow* w) { m_window = w; }

    void toggleMaximize();
    void toggleFullscreen();
    // Call each frame BEFORE ImGui::NewFrame so window size is correct this frame.
    void tick();

    bool isMaximized()  const { return m_isMaximized; }
    bool isFullscreen() const { return m_isFullscreen; }

private:
    GLFWwindow* m_window {nullptr};

    bool m_isMaximized  {false};
    int  m_restoreX {0}, m_restoreY {0}, m_restoreW {1280}, m_restoreH {800};

    bool m_isFullscreen {false};
    int  m_fsRestoreX {0}, m_fsRestoreY {0}, m_fsRestoreW {1280}, m_fsRestoreH {800};

    enum class ResizeEdge { None, Right, Bottom, BottomRight };
    ResizeEdge m_resizeEdge {ResizeEdge::None};
    int  m_rsMouseX {0}, m_rsMouseY {0};
    int  m_rsWinW   {0}, m_rsWinH   {0};
    bool m_prevLmb  {false};
};
