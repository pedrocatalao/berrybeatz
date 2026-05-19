#include "App.h"
#include "FileDialog.h"
#include "Logo.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <libopenmpt/libopenmpt_ext.hpp>
#include <cmath>
#include <algorithm>
#include <cstdio>

// ─── Constructor / Destructor ─────────────────────────────────────────────────

App::App() : m_audio(m_state), m_tracker(m_state)
{
    // ── Toolbar callbacks ──────────────────────────────────────────────────
    // ── Window control callbacks ───────────────────────────────────────────
    m_toolbarState.onWindowClose    = [this]() { glfwSetWindowShouldClose(m_glfwWindow, 1); };
    m_toolbarState.onWindowMinimize = [this]() { glfwIconifyWindow(m_glfwWindow); };
    m_toolbarState.onWindowMaximize = [this]() {
        // glfwMaximizeWindow / GLFW_MAXIMIZED don't work for borderless windows
        // on macOS (zoom: is a no-op without NSWindowStyleMaskResizable).
        // Track state ourselves and fill the monitor work area instead.
        if (m_isMaximized) {
            glfwSetWindowPos (m_glfwWindow, m_restoreX, m_restoreY);
            glfwSetWindowSize(m_glfwWindow, m_restoreW, m_restoreH);
            m_isMaximized = false;
        } else {
            glfwGetWindowPos (m_glfwWindow, &m_restoreX, &m_restoreY);
            glfwGetWindowSize(m_glfwWindow, &m_restoreW, &m_restoreH);

            // Find which monitor the window centre is on
            int wcx = m_restoreX + m_restoreW / 2;
            int wcy = m_restoreY + m_restoreH / 2;
            int mcount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&mcount);
            GLFWmonitor* target = glfwGetPrimaryMonitor();
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

            int ax, ay, aw, ah;
            glfwGetMonitorWorkarea(target, &ax, &ay, &aw, &ah);
            glfwSetWindowPos (m_glfwWindow, ax, ay);
            glfwSetWindowSize(m_glfwWindow, aw, ah);
            m_isMaximized = true;
        }
    };

    m_toolbarState.onWindowFullscreen = [this]() {
        if (m_isFullscreen) {
            glfwSetWindowMonitor(m_glfwWindow, nullptr,
                                 m_fsRestoreX, m_fsRestoreY,
                                 m_fsRestoreW, m_fsRestoreH, 0);
            m_isFullscreen = false;
        } else {
            glfwGetWindowPos (m_glfwWindow, &m_fsRestoreX, &m_fsRestoreY);
            glfwGetWindowSize(m_glfwWindow, &m_fsRestoreW, &m_fsRestoreH);

            int wcx = m_fsRestoreX + m_fsRestoreW / 2;
            int wcy = m_fsRestoreY + m_fsRestoreH / 2;
            int mcount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&mcount);
            GLFWmonitor* target = glfwGetPrimaryMonitor();
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

            const GLFWvidmode* vm = glfwGetVideoMode(target);
            glfwSetWindowMonitor(m_glfwWindow, target,
                                 0, 0, vm->width, vm->height, vm->refreshRate);
            m_isFullscreen = true;
            m_isMaximized  = false;
        }
    };

    m_toolbarState.onNew  = [this]() { clearModule(); };
    m_toolbarState.onOpen = [this]() { openFileDialog(); };
    m_toolbarState.onSave = [this]() { /* TODO: save */ };
    m_toolbarState.onStop = [this]() {
        if (m_audio.isRunning()) {
            m_audio.stop();
            m_audio.join();
        }
        if (auto* mod = m_audio.getModule())
            mod->set_position_seconds(0.0);
    };
    m_toolbarState.onPlayPause     = [this]() { togglePlayPause(); };
    m_toolbarState.onPlayFromStart = [this]() { playFromStart(); };
    m_toolbarState.onSeek          = [this](float f) { seekTo(f); };

    m_toolbarState.onOctaveChange = [this](int v) {
        m_tracker.setBaseOctave(v);
    };
    m_toolbarState.onTempoChange = [this](int v) {
        auto* mod = m_audio.getModule();
        if (!mod) return;
        auto* ia = static_cast<openmpt::ext::interactive*>(
            mod->get_interface(openmpt::ext::interactive_id));
        // set_current_tempo is deprecated but still the only per-song API available
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
        ia->set_current_tempo(v);
        #pragma clang diagnostic pop
    };
    m_toolbarState.onSpeedChange = [this](int v) {
        auto* mod = m_audio.getModule();
        if (!mod) return;
        auto* ia = static_cast<openmpt::ext::interactive*>(
            mod->get_interface(openmpt::ext::interactive_id));
        if (ia) ia->set_current_speed(v);
    };
    m_toolbarState.onRPBChange       = [](int) {};  // determined by pattern data
    m_toolbarState.onGlobalVolChange = [this](int v) {
        auto* mod = m_audio.getModule();
        if (!mod) return;
        auto* ia = static_cast<openmpt::ext::interactive*>(
            mod->get_interface(openmpt::ext::interactive_id));
        ia->set_global_volume((double)v / 256.0);
    };

    // ── AudioEngine callbacks ──────────────────────────────────────────────
    m_audio.onLoadFailed = [](const std::string& msg) {
        fprintf(stderr, "Load failed: %s\n", msg.c_str());
    };
    m_audio.onSongFinished = [this]() {
        // Called from audio thread — just note playback ended naturally
        (void)this;
    };

    // ── TrackerView callbacks ──────────────────────────────────────────────
    m_tracker.onOrderSeekRequested = [this](int32_t order) {
        m_audio.seekToOrder(order);
        if (!m_audio.isRunning() && m_hasModule)
            startPlayback();
    };
    m_tracker.onPatternPlayFromCursor = [this](int32_t order, int32_t row, bool loop) {
        m_audio.playPatternFrom(order, row, loop);
    };
    m_tracker.onPatternPlayFromStart = [this](int32_t order, bool loop) {
        m_audio.playPatternFrom(order, 0, loop);
    };
    m_tracker.onPatternPlayRow = [this](int32_t order, int32_t row) {
        m_audio.playRow(order, row);
    };
    m_tracker.onStopRequested = [this]() { stopPlayback(); };

}

App::~App()
{
    m_audio.stop();
    m_audio.join();
    if (m_logoTex) glDeleteTextures(1, &m_logoTex);
}

// ─── Public API ───────────────────────────────────────────────────────────────

void App::setGLFWWindow(GLFWwindow* w)
{
    m_glfwWindow = w;
    m_logoTex = loadLogoTexture(m_logoW, m_logoH);
}

void App::setFonts(ImFont* mono, ImFont* ui)
{
    m_fontMono = mono;
    m_fontUI   = ui;
    m_tracker.setFonts(mono, ui);
}


void App::openFile(const std::string& path)
{
    clearModule();
    if (m_audio.loadFile(path)) {
        m_hasModule  = true;
        m_loadedPath = path;
        m_tracker.setModule(m_audio.getModule());
        m_toolbarState.tempo = (int)std::round(m_state.bpm.load());
        m_toolbarState.speed = (int)m_state.current_speed.load();
        m_toolbarState.rpb   = (int)m_state.rows_per_beat.load();
        startPlayback();
    }
}

// ─── Render ───────────────────────────────────────────────────────────────────

void App::render(int winW, int winH)
{
    const float w = (float)winW;
    const float h = (float)winH;

    syncToolbarState();

    float contentY = DrawToolbar(m_toolbarState, m_knobEdit,
                                 m_fontUI, 0.f, 0.f, w, kToolbarH);

    ImGui::PushFont(m_fontUI);

    // ── Sidebar ───────────────────────────────────────────────────────────
    drawSidebar(0.f, contentY, m_sidebarW, h - contentY);

    // ── Splitter (drag handle) ────────────────────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));  // transparent
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::SetNextWindowPos({m_sidebarW, contentY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({kSplitterW, h - contentY}, ImGuiCond_Always);
        ImGui::Begin("##splitter", nullptr,
            ImGuiWindowFlags_NoDecoration      |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoMove            |
            ImGuiWindowFlags_NoSavedSettings   |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // Draw 1px line at centre of the hit area
        ImVec2 wp = ImGui::GetWindowPos();
        float  cx = wp.x + kSplitterW * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(
            {cx, wp.y}, {cx, wp.y + h - contentY},
            IM_COL32(55, 55, 80, 255), 1.f);

        ImGui::InvisibleButton("##drag", ImGui::GetContentRegionAvail());
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive()) {
            m_sidebarW = std::clamp(m_sidebarW + ImGui::GetIO().MouseDelta.x, 100.f, 500.f);
        }
        ImGui::End();
    }

    // Content starts to the right of sidebar + splitter
    const float cx = m_sidebarW + kSplitterW;
    const float cw = w - cx;

    // ── Tab bar ───────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_WindowBg,  IM_COL32(20, 20, 40, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(4.f, 2.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowPos({cx, contentY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({cw, kTabBarH}, ImGuiCond_Always);
    ImGui::Begin("##tabbar", nullptr,
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    if (ImGui::BeginTabBar("##tabs")) {
        const char* tabs[] = {"General", "Patterns", "Samples", "Instruments", "Comments"};
        for (int i = 0; i < 5; ++i) {
            ImGuiTabItemFlags flags = (!m_tabsInited && i == m_activeTab)
                                      ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(tabs[i], nullptr, flags)) {
                m_activeTab = i;
                ImGui::EndTabItem();
            }
        }
        m_tabsInited = true;
        ImGui::EndTabBar();
    }
    ImGui::End();

    // ── Content area ──────────────────────────────────────────────────────
    float cy = contentY + kTabBarH;
    float ch = h - cy;

    if (m_activeTab == 1) {
        m_tracker.render(cx, cy, cw, ch);
    } else {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 20, 32, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(12.f, 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::SetNextWindowPos({cx, cy}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({cw, ch}, ImGuiCond_Always);
        ImGui::Begin("##content", nullptr,
            ImGuiWindowFlags_NoDecoration      |
            ImGuiWindowFlags_NoMove            |
            ImGuiWindowFlags_NoSavedSettings   |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        switch (m_activeTab) {
        case 0: drawGeneralTab();      break;
        case 2: drawSamplesTab();      break;
        case 3: drawInstrumentsTab();  break;
        case 4: drawCommentsTab();     break;
        }

        ImGui::End();
    }

    drawKnobEditModal();
    ImGui::PopFont();
    handleEdgeResize(winW, winH);
}

// ─── Sidebar (module tree) ────────────────────────────────────────────────────

void App::drawSidebar(float x, float y, float w, float h)
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg,             IM_COL32(20, 20, 40, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          IM_COL32(18, 18, 30, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        IM_COL32(55, 55, 85, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(75, 75, 110, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  IM_COL32(95, 95, 140, 255));
    ImGui::PushStyleColor(ImGuiCol_Header,               IM_COL32(45, 45, 75, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,        IM_COL32(60, 60, 95, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,         IM_COL32(75, 75, 115, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,     ImVec2(6.f, 4.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,       ImVec2(4.f, 2.f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing,     12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,  0.f);

    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);
    ImGui::Begin("##sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar    |
        ImGuiWindowFlags_NoResize      |
        ImGuiWindowFlags_NoCollapse    |
        ImGuiWindowFlags_NoMove        |
        ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(8);

    if (!m_hasModule) {
        ImGui::TextDisabled("No module loaded");
        ImGui::End();
        return;
    }

    // Module title
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        const char* title = m_state.title.empty() ? "(untitled)" : m_state.title.c_str();
        ImGui::TextColored({0.75f, 0.75f, 1.f, 1.f}, "%s", title);
    }
    ImGui::Separator();

    // Helper: draw a numbered list under a collapsing section.
    // Returns the index that was clicked, or -1.
    auto section = [&](const char* label,
                        const std::vector<std::string>& names,
                        int tabSwitch) -> int
    {
        char header[64];
        snprintf(header, sizeof(header), "%s (%d)##hdr", label, (int)names.size());

        int clicked = -1;
        ImGuiTreeNodeFlags nodeFlags =
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (ImGui::TreeNodeEx(header, nodeFlags)) {
            for (int i = 0; i < (int)names.size(); ++i) {
                ImGui::PushID(i);
                char row[256];
                snprintf(row, sizeof(row), "%3d  %s", i + 1,
                         names[i].empty() ? "---" : names[i].c_str());
                if (ImGui::Selectable(row, false, 0, {w - 24.f, 0.f})) {
                    clicked = i;
                    m_activeTab = tabSwitch;
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        return clicked;
    };

    // Patterns use 0-based numbering and a different tab index
    auto patSection = [&]() {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        char header[64];
        snprintf(header, sizeof(header), "Patterns (%d)##hdr", m_state.num_patterns);

        ImGuiTreeNodeFlags nodeFlags =
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (ImGui::TreeNodeEx(header, nodeFlags)) {
            for (int i = 0; i < m_state.num_patterns; ++i) {
                ImGui::PushID(i + 10000);
                char row[256];
                const std::string& name =
                    (i < (int)m_state.pattern_names.size()) ? m_state.pattern_names[i] : "";
                snprintf(row, sizeof(row), "%3d  %s", i,
                         name.empty() ? "---" : name.c_str());
                if (ImGui::Selectable(row, false, 0, {w - 24.f, 0.f}))
                    m_activeTab = 1;  // switch to Patterns tab
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    };

    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        if (!m_state.instrument_names.empty()) {
            // release lock while rendering, re-acquire via copies
        }
    }

    // Render sections with brief locks to copy the name vectors
    std::vector<std::string> instrNames, sampleNames;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        instrNames  = m_state.instrument_names;
        sampleNames = m_state.sample_names;
    }

    if (!instrNames.empty())
        section("Instruments", instrNames, 3);

    section("Samples", sampleNames, 2);
    patSection();

    ImGui::End();
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void App::syncToolbarState()
{
    m_toolbarState.logoTex = m_logoTex;
    m_toolbarState.logoW   = m_logoW;
    m_toolbarState.logoH   = m_logoH;
    m_toolbarState.hasModule = m_hasModule;
    m_toolbarState.isPlaying = m_state.is_playing.load();
    m_toolbarState.isPaused  = m_audio.isPaused();
    m_toolbarState.vuLeft    = m_state.master_vu_left.load();
    m_toolbarState.vuRight   = m_state.master_vu_right.load();

    if (m_hasModule) {
        double dur = m_state.duration_seconds.load();
        double pos = m_state.position_seconds.load();
        m_toolbarState.seekFrac = (dur > 0.0) ? (float)(pos / dur) : 0.f;
        m_toolbarState.tempo    = (int)std::round(m_state.bpm.load());
        m_toolbarState.speed    = (int)m_state.current_speed.load();
        m_toolbarState.rpb      = (int)m_state.rows_per_beat.load();
    }
}

void App::drawKnobEditModal()
{
    if (m_knobEdit.pending) {
        ImGui::OpenPopup("##knob_edit");
        m_knobEdit.pending = false;
        ImGui::SetNextWindowPos(
            {m_knobEdit.popupX, m_knobEdit.popupY}, ImGuiCond_Appearing);
        m_editValue = m_knobEdit.value;
    }

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
    if (ImGui::BeginPopupModal("##knob_edit", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {

        ImGui::TextUnformatted(m_knobEdit.label.c_str());
        ImGui::SetNextItemWidth(90.f);
        ImGui::InputInt("##v", &m_editValue, 1, 10);
        m_editValue = std::clamp(m_editValue, m_knobEdit.vmin, m_knobEdit.vmax);

        bool confirm = ImGui::Button("OK", {44.f, 0.f});
        confirm |= ImGui::IsKeyPressed(ImGuiKey_Enter, false);
        if (confirm) {
            if (m_knobEdit.onConfirm) m_knobEdit.onConfirm(m_editValue);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {56.f, 0.f}) ||
                ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

// ─── Tab content ──────────────────────────────────────────────────────────────

void App::drawGeneralTab()
{
    if (!m_hasModule) {
        ImGui::TextDisabled("No module loaded.");
        return;
    }
    std::lock_guard<std::mutex> lk(m_state.meta_mutex);
    ImGui::Text("Title:       %s", m_state.title.c_str());
    ImGui::Text("Artist:      %s", m_state.artist.c_str());
    ImGui::Text("Format:      %s", m_state.type_long.c_str());
    ImGui::Text("Tracker:     %s", m_state.tracker.c_str());
    ImGui::Separator();
    ImGui::Text("Orders:      %d", (int)m_state.num_orders.load());
    ImGui::Text("Patterns:    %d", m_state.num_patterns);
    ImGui::Text("Channels:    %d", (int)m_state.num_channels.load());
    ImGui::Text("Samples:     %d", m_state.num_samples);
    ImGui::Text("Instruments: %d", m_state.num_instruments);
}

void App::drawSamplesTab()
{
    if (!m_hasModule) { ImGui::TextDisabled("No module loaded."); return; }
    std::lock_guard<std::mutex> lk(m_state.meta_mutex);
    for (int i = 0; i < (int)m_state.sample_names.size(); ++i)
        ImGui::Text("%3d. %s", i + 1, m_state.sample_names[i].c_str());
}

void App::drawInstrumentsTab()
{
    if (!m_hasModule) { ImGui::TextDisabled("No module loaded."); return; }
    std::lock_guard<std::mutex> lk(m_state.meta_mutex);
    if (m_state.instrument_names.empty()) {
        ImGui::TextDisabled("No instruments (sample-based module).");
        return;
    }
    for (int i = 0; i < (int)m_state.instrument_names.size(); ++i)
        ImGui::Text("%3d. %s", i + 1, m_state.instrument_names[i].c_str());
}

void App::drawCommentsTab()
{
    if (!m_hasModule) { ImGui::TextDisabled("No module loaded."); return; }
    auto* mod = m_audio.getModule();
    if (!mod) return;
    static std::string msg;
    static bool msgLoaded = false;
    if (!msgLoaded) {
        msg       = mod->get_metadata("message");
        msgLoaded = true;
    }
    // Reset on module change would clear msgLoaded — handled in clearModule()
    ImGui::InputTextMultiline("##msg", msg.data(), msg.size() + 1,
                              {-1.f, -1.f}, ImGuiInputTextFlags_ReadOnly);
}

// ─── Edge resize (GLFW-based, bypasses ImGui z-order) ────────────────────────

void App::handleEdgeResize(int winW, int winH)
{
    if (!m_glfwWindow) return;

    const int kE = 6;   // edge grab pixels
    const int kC = 14;  // corner grab size

    double mx, my;
    glfwGetCursorPos(m_glfwWindow, &mx, &my);
    int ix = (int)mx, iy = (int)my;

    bool nearRight  = (ix >= winW - kE);
    bool nearBottom = (iy >= winH - kE);
    bool nearCorner = (ix >= winW - kC && iy >= winH - kC);

    int lmb = glfwGetMouseButton(m_glfwWindow, GLFW_MOUSE_BUTTON_LEFT);

    if (m_resizeEdge == ResizeEdge::None) {
        // Show cursor hint when hovering an edge
        if (nearCorner)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
        else if (nearRight)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        else if (nearBottom)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        // Capture drag start
        if (lmb == GLFW_PRESS && (nearRight || nearBottom)) {
            int wx, wy;
            glfwGetWindowPos(m_glfwWindow, &wx, &wy);
            m_rsMouseX = wx + ix;
            m_rsMouseY = wy + iy;
            glfwGetWindowSize(m_glfwWindow, &m_rsWinW, &m_rsWinH);

            if (nearCorner)     m_resizeEdge = ResizeEdge::BottomRight;
            else if (nearRight) m_resizeEdge = ResizeEdge::Right;
            else                m_resizeEdge = ResizeEdge::Bottom;
        }
    } else {
        if (lmb == GLFW_PRESS) {
            // Keep cursor while dragging
            if      (m_resizeEdge == ResizeEdge::Right)       ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            else if (m_resizeEdge == ResizeEdge::Bottom)      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            else                                              ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

            int wx, wy;
            glfwGetWindowPos(m_glfwWindow, &wx, &wy);
            int dx = (wx + ix) - m_rsMouseX;
            int dy = (wy + iy) - m_rsMouseY;

            int newW = m_rsWinW, newH = m_rsWinH;
            if (m_resizeEdge == ResizeEdge::Right || m_resizeEdge == ResizeEdge::BottomRight)
                newW = std::max(400, m_rsWinW + dx);
            if (m_resizeEdge == ResizeEdge::Bottom || m_resizeEdge == ResizeEdge::BottomRight)
                newH = std::max(300, m_rsWinH + dy);

            glfwSetWindowSize(m_glfwWindow, newW, newH);
        } else {
            m_resizeEdge = ResizeEdge::None;
        }
    }
}

// ─── Operations ───────────────────────────────────────────────────────────────

void App::openFileDialog()
{
    std::string path = nativeOpenFileDialog("Open Module");
    if (!path.empty()) openFile(path);
}

void App::clearModule()
{
    m_audio.stop();
    m_audio.join();
    m_tracker.clearModule();
    m_state.reset();
    m_hasModule  = false;
    m_loadedPath.clear();
}

void App::startPlayback()
{
    if (!m_hasModule || m_audio.isRunning()) return;
    m_audio.setPaused(false);
    m_audio.start();
}

void App::stopPlayback()
{
    if (!m_audio.isRunning()) return;
    m_audio.stop();
    m_audio.join();
}

void App::togglePlayPause()
{
    if (!m_hasModule) return;
    if (!m_audio.isRunning()) {
        startPlayback();
    } else if (m_audio.isPaused()) {
        m_audio.setPaused(false);
    } else {
        m_audio.setPaused(true);
    }
}

void App::playFromStart()
{
    if (!m_hasModule) return;
    m_audio.rewind();
    m_audio.start();
}

void App::seekTo(float frac)
{
    double dur = m_state.duration_seconds.load();
    m_audio.seekSeconds(frac * dur);
    if (!m_audio.isRunning() && m_hasModule)
        startPlayback();
}
