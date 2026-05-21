#include "WindowChrome.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <algorithm>

static GLFWmonitor* monitorForWindow(GLFWwindow* w)
{
    int wx, wy, ww, wh;
    glfwGetWindowPos (w, &wx, &wy);
    glfwGetWindowSize(w, &ww, &wh);
    int wcx = wx + ww / 2, wcy = wy + wh / 2;

    int mcount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&mcount);
    GLFWmonitor*  target   = glfwGetPrimaryMonitor();
    for (int i = 0; i < mcount; ++i) {
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const GLFWvidmode* vm = glfwGetVideoMode(monitors[i]);
        if (wcx >= mx && wcx < mx + vm->width &&
            wcy >= my && wcy < my + vm->height) {
            target = monitors[i];
            break;
        }
    }
    return target;
}

void WindowChrome::toggleMaximize()
{
    if (!m_window) return;
    if (m_isMaximized) {
        glfwSetWindowPos (m_window, m_restoreX, m_restoreY);
        glfwSetWindowSize(m_window, m_restoreW, m_restoreH);
        m_isMaximized = false;
    } else {
        glfwGetWindowPos (m_window, &m_restoreX, &m_restoreY);
        glfwGetWindowSize(m_window, &m_restoreW, &m_restoreH);
        int ax, ay, aw, ah;
        glfwGetMonitorWorkarea(monitorForWindow(m_window), &ax, &ay, &aw, &ah);
        glfwSetWindowPos (m_window, ax, ay);
        glfwSetWindowSize(m_window, aw, ah);
        m_isMaximized = true;
    }
}

void WindowChrome::toggleFullscreen()
{
    if (!m_window) return;
    if (m_isFullscreen) {
        glfwSetWindowMonitor(m_window, nullptr,
                             m_fsRestoreX, m_fsRestoreY,
                             m_fsRestoreW, m_fsRestoreH, 0);
        m_isFullscreen = false;
    } else {
        glfwGetWindowPos (m_window, &m_fsRestoreX, &m_fsRestoreY);
        glfwGetWindowSize(m_window, &m_fsRestoreW, &m_fsRestoreH);
        GLFWmonitor*       target = monitorForWindow(m_window);
        const GLFWvidmode* vm     = glfwGetVideoMode(target);
        glfwSetWindowMonitor(m_window, target,
                             0, 0, vm->width, vm->height, vm->refreshRate);
        m_isFullscreen = true;
        m_isMaximized  = false;
    }
}

void WindowChrome::tick()
{
    if (!m_window) return;

    const int kE = 6;   // edge grab pixels
    const int kC = 14;  // corner grab size

    int winW, winH;
    glfwGetWindowSize(m_window, &winW, &winH);

    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);
    int ix = (int)mx, iy = (int)my;

    bool nearRight  = (ix >= winW - kE);
    bool nearBottom = (iy >= winH - kE);
    bool nearCorner = (ix >= winW - kC && iy >= winH - kC);

    int  lmb        = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
    bool pressed    = (lmb == GLFW_PRESS);
    bool justPressed = pressed && !m_prevLmb;
    m_prevLmb = pressed;

    if (m_resizeEdge == ResizeEdge::None) {
        if (nearCorner)      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
        else if (nearRight)  ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        else if (nearBottom) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        // Only start a drag on the exact frame the button goes down near an edge,
        // so a click held inside the app can't accidentally start a resize.
        if (justPressed && (nearRight || nearBottom)) {
            int wx, wy;
            glfwGetWindowPos(m_window, &wx, &wy);
            m_rsMouseX = wx + ix;
            m_rsMouseY = wy + iy;
            glfwGetWindowSize(m_window, &m_rsWinW, &m_rsWinH);

            if (nearCorner)     m_resizeEdge = ResizeEdge::BottomRight;
            else if (nearRight) m_resizeEdge = ResizeEdge::Right;
            else                m_resizeEdge = ResizeEdge::Bottom;
        }
    } else {
        if (pressed) {
            if      (m_resizeEdge == ResizeEdge::Right)  ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            else if (m_resizeEdge == ResizeEdge::Bottom) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            else                                         ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

            int wx, wy;
            glfwGetWindowPos(m_window, &wx, &wy);
            int dx = (wx + ix) - m_rsMouseX;
            int dy = (wy + iy) - m_rsMouseY;

            int newW = m_rsWinW, newH = m_rsWinH;
            if (m_resizeEdge == ResizeEdge::Right || m_resizeEdge == ResizeEdge::BottomRight)
                newW = std::max(400, m_rsWinW + dx);
            if (m_resizeEdge == ResizeEdge::Bottom || m_resizeEdge == ResizeEdge::BottomRight)
                newH = std::max(300, m_rsWinH + dy);

            glfwSetWindowSize(m_window, newW, newH);
        } else {
            m_resizeEdge = ResizeEdge::None;
        }
    }
}
