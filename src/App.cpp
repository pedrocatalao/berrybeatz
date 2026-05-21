#include "App.h"
#include "CocoaHelpers.h"
#include "Logo.h"
#include "OpenMPTInternal.h"
#include "Knob.h"
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
    m_toolbarState.onSave = [this]() { saveModule(); };
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
    m_tracker.onInsertPatternRequested = [this]() { insertPattern(); };

}

App::~App()
{
    m_audio.stop();
    m_audio.join();
    if (m_logoTex)        glDeleteTextures(1, &m_logoTex);
    if (m_speakerConeTex) glDeleteTextures(1, &m_speakerConeTex);
}

// ─── Public API ───────────────────────────────────────────────────────────────

void App::setGLFWWindow(GLFWwindow* w)
{
    m_glfwWindow = w;
    m_logoTex        = loadLogoTexture(m_logoW, m_logoH);
    m_speakerConeTex = loadSpeakerConeTexture(m_speakerConeW, m_speakerConeH);
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

    float contentY = DrawToolbar(m_toolbarState,
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


        ImGui::InvisibleButton("##drag", ImGui::GetContentRegionAvail());
        {
            float mx = ImGui::GetIO().MousePos.x;
            bool inBand = (mx >= m_sidebarW && mx < m_sidebarW + kSplitterW);
            if (inBand || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
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

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   ImVec2(8.5f, 4.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(1.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Tab,        IM_COL32(10, 10, 22, 255));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, IM_COL32(35, 35, 60, 255));
    if (ImGui::BeginTabBar("##tabs")) {
        const char* tabs[] = {"General", "Patterns", "Samples", "Instruments", "Comments"};
        for (int i = 0; i < 5; ++i) {
            bool active = (i == m_activeTab);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? IM_COL32(208, 208, 224, 255)
                                                        : IM_COL32(110, 110, 130, 255));
            ImGuiTabItemFlags flags = (!m_tabsInited && i == m_activeTab)
                                      ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(tabs[i], nullptr, flags)) {
                m_activeTab = i;
                ImGui::EndTabItem();
            }
            ImGui::PopStyleColor();
        }
        m_tabsInited = true;
        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::End();

    // Extend the tab bar separator to the left edge of the window.
    // The bar's inner rect starts at cx + tabBarPaddingX, so overlap by that amount.
    {
        constexpr float tabBarPadX = 1.f;  // WindowPadding.x used for ##tabbar
        constexpr float tabBarPadY = 2.f;  // WindowPadding.y used for ##tabbar
        ImVec4 c   = ImGui::GetStyle().Colors[ImGuiCol_TabSelected];
        ImU32  col = ImGui::ColorConvertFloat4ToU32(c);
        float  lineY = contentY + kTabBarH - tabBarPadY - 2.f;
        ImGui::GetForegroundDrawList()->AddLine({0.f, lineY}, {cx + tabBarPadX, lineY}, col, 1.f);
    }

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

    ImGui::PopFont();
    handleEdgeResize(winW, winH);
}

// ─── Sidebar (module tree) ────────────────────────────────────────────────────

void App::drawSidebar(float x, float y, float w, float h)
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      IM_COL32(20, 20, 40, 255));
    ImGui::PushStyleColor(ImGuiCol_Header,        IM_COL32(45, 45, 75, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(60, 60, 95, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  IM_COL32(75, 75, 115, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(8.f, 4.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(4.f, 2.f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing,    12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);
    ImGui::Begin("##sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar              |
        ImGuiWindowFlags_NoResize                |
        ImGuiWindowFlags_NoCollapse              |
        ImGuiWindowFlags_NoMove                  |
        ImGuiWindowFlags_NoSavedSettings         |
        ImGuiWindowFlags_NoScrollbar             |
        ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(4);

    if (!m_hasModule) {
        ImGui::TextDisabled("No module loaded");
        ImGui::End();
        return;
    }

    // ── Sticky header (never scrolls) ─────────────────────────────────────────
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        const char* title = m_state.title.empty() ? "(untitled)" : m_state.title.c_str();
        ImGui::TextColored({0.75f, 0.75f, 1.f, 1.f}, "%s", title);
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);

    // ── Scrollable list ────────────────────────────────────────────────────────
    // Extend child into the parent's right padding so the scrollbar sits flush
    // against the sidebar edge.  Zero child padding so no extra gaps are added.
    // Scrollbar style is pushed here (not on parent) and popped after EndChild
    // so it is still active when ImGui renders the scrollbar in EndChild.
    {
        const float rp    = ImGui::GetStyle().WindowPadding.x;
        const float sw    = ImGui::GetStyle().ScrollbarSize;
        const float childW = ImGui::GetContentRegionAvail().x + rp;
        ImGui::SetNextWindowContentSize({childW - sw, 0.f});
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          IM_COL32( 12,  12,  22, 255));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        IM_COL32( 40,  85, 145, 200));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32( 55, 112, 175, 230));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  IM_COL32( 66, 130, 205, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##sidebar_scroll", {childW, 0.f}, false, 0);
        ImGui::PopStyleVar(1);  // WindowPadding — no longer needed after Begin
    }

    // Helper: draw a numbered list under a collapsing section.
    // Returns the index that was clicked, or -1.
    auto section = [&](const char* label,
                        const std::vector<std::string>& names,
                        int tabSwitch,
                        int activeSel = -1) -> int
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
                bool isActive = (i + 1 == activeSel);
                if (ImGui::Selectable(row, isActive, 0, {0.f, 0.f})) {
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
                if (ImGui::Selectable(row, false, 0, {0.f, 0.f}))
                    m_activeTab = 1;
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    };

    // Render sections with brief locks to copy the name vectors
    std::vector<std::string> instrNames, sampleNames;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        instrNames  = m_state.instrument_names;
        sampleNames = m_state.sample_names;
    }

    if (!instrNames.empty()) {
        int clicked = section("Instruments", instrNames, 3, m_instrView.getSelection());
        if (clicked >= 0)
            m_instrView.setSelection(clicked + 1);
    }

    {
        int clicked = section("Samples", sampleNames, 2, m_sampleView.getSelection());
        if (clicked >= 0)
            m_sampleView.setSelection(clicked + 1);
    }
    patSection();

    ImGui::EndChild();
    ImGui::PopStyleColor(4); // scrollbar colors

    ImGui::End();
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void App::syncToolbarState()
{
    m_toolbarState.logoTex       = m_logoTex;
    m_toolbarState.logoW         = m_logoW;
    m_toolbarState.logoH         = m_logoH;
    m_toolbarState.speakerConeTex = m_speakerConeTex;
    m_toolbarState.speakerConeW   = m_speakerConeW;
    m_toolbarState.speakerConeH   = m_speakerConeH;
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

// ─── Tab content ──────────────────────────────────────────────────────────────

void App::drawGeneralTab()
{
    if (!m_hasModule) {
        ImVec2 av = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos({av.x * 0.5f - 50.f, av.y * 0.5f - 8.f});
        ImGui::TextDisabled("No module loaded.");
        return;
    }

    auto* mod = m_audio.getModule();
    auto* ia  = mod ? static_cast<openmpt::ext::interactive*>(
                    mod->get_interface(openmpt::ext::interactive_id)) : nullptr;
    auto* ia3 = mod ? static_cast<openmpt::ext::interactive3*>(
                    mod->get_interface(openmpt::ext::interactive3_id)) : nullptr;

    const ImVec4 kHdr = {0.55f, 0.75f, 1.00f, 1.f};
    const ImVec4 kDim = {0.50f, 0.50f, 0.65f, 1.f};
    const ImVec4 kVal = {0.82f, 0.82f, 0.88f, 1.f};

    // ── Module info ──────────────────────────────────────────────────────────
    char titleBuf[256] = {}, artistBuf[256] = {};
    std::string type_long, tracker;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        strncpy(titleBuf,  m_state.title.c_str(),  255);
        strncpy(artistBuf, m_state.artist.c_str(), 255);
        type_long = m_state.type_long;
        tracker   = m_state.tracker;
    }

    if (ImGui::BeginTable("##geninfotbl", 2, ImGuiTableFlags_None)) {
        ImGui::TableSetupColumn("##lbl", ImGuiTableColumnFlags_WidthFixed, 52.f);
        ImGui::TableSetupColumn("##val", ImGuiTableColumnFlags_WidthStretch);

        auto infoRow = [&](const char* lbl, const char* id, char* buf, int bufsz) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kDim, "%s", lbl);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputText(id, buf, bufsz, ImGuiInputTextFlags_ReadOnly);
        };
        infoRow("Title",  "##gentitle",  titleBuf,  256);
        infoRow("Artist", "##genartist", artistBuf, 256);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kDim, "Format");
        ImGui::TableSetColumnIndex(1);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kVal, "%s", type_long.c_str());
        if (!tracker.empty()) {
            ImGui::SameLine(0, 12);
            ImGui::TextColored(kDim, "|");
            ImGui::SameLine(0, 12);
            ImGui::TextColored(kVal, "%s", tracker.c_str());
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // ── Stats row ────────────────────────────────────────────────────────────
    {
        int orders = (int)m_state.num_orders.load();
        int channels = (int)m_state.num_channels.load();
        int patterns, samples, instruments;
        {
            std::lock_guard<std::mutex> lk(m_state.meta_mutex);
            patterns    = m_state.num_patterns;
            samples     = m_state.num_samples;
            instruments = m_state.num_instruments;
        }
        double dur = m_state.duration_seconds.load();
        int mm = (int)dur / 60, ss = (int)dur % 60;

        ImGui::TextColored(kDim, "Duration"); ImGui::SameLine(0, 3);
        ImGui::TextColored(kVal, "%d:%02d", mm, ss);
        ImGui::SameLine(0, 14);
        ImGui::TextColored(kDim, "Orders"); ImGui::SameLine(0, 3);
        ImGui::TextColored(kVal, "%d", orders);
        ImGui::SameLine(0, 14);
        ImGui::TextColored(kDim, "Patterns"); ImGui::SameLine(0, 3);
        ImGui::TextColored(kVal, "%d", patterns);
        ImGui::SameLine(0, 14);
        ImGui::TextColored(kDim, "Channels"); ImGui::SameLine(0, 3);
        ImGui::TextColored(kVal, "%d", channels);
        ImGui::SameLine(0, 14);
        ImGui::TextColored(kDim, "Samples"); ImGui::SameLine(0, 3);
        ImGui::TextColored(kVal, "%d", samples);
        if (instruments > 0) {
            ImGui::SameLine(0, 14);
            ImGui::TextColored(kDim, "Instruments"); ImGui::SameLine(0, 3);
            ImGui::TextColored(kVal, "%d", instruments);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Two-column: Playback | Render ────────────────────────────────────────
    float avW  = ImGui::GetContentRegionAvail().x;
    float avH  = ImGui::GetContentRegionAvail().y - 2.f;
    float colW = (avW - 12.f) * 0.5f;

    // ── Playback panel ───────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 14, 26, 255));
    if (ImGui::BeginChild("##genplay", {colW, avH}, true)) {
        ImGui::TextColored(kHdr, "PLAYBACK");
        ImGui::Separator();
        ImGui::Spacing();

        if (mod && ia && ia3) {
            int tempo = (int)std::round(mod->get_current_tempo2());
            int speed = (int)mod->get_current_speed();
            int gvol  = (int)std::round(ia->get_global_volume() * 100.0);

            // Knob + centered label + value below
            auto kGroup = [&](const char* kid, int* v, int vmin, int vmax,
                               const char* lbl) -> bool {
                constexpr float KW = 36.f;
                ImVec2 cp = ImGui::GetCursorPos();
                bool ch = KnobWidget(kid, v, vmin, vmax);
                float lw = ImGui::CalcTextSize(lbl).x;
                ImGui::SetCursorPosX(cp.x + (KW - lw) * 0.5f);
                ImGui::TextColored(kDim, "%s", lbl);
                char vbuf[16]; snprintf(vbuf, sizeof(vbuf), "%d", *v);
                float vw = ImGui::CalcTextSize(vbuf).x;
                ImGui::SetCursorPosX(cp.x + (KW - vw) * 0.5f);
                ImGui::TextColored(kVal, "%s", vbuf);
                return ch;
            };

            ImGui::BeginGroup();
            if (kGroup("##kTempo", &tempo, 32, 512, "Tempo"))
                ia3->set_current_tempo2((double)tempo);
            ImGui::EndGroup();
            ImGui::SameLine(0, 16);
            ImGui::BeginGroup();
            if (kGroup("##kSpeed", &speed, 1, 32, "Speed"))
                ia->set_current_speed(speed);
            ImGui::EndGroup();
            ImGui::SameLine(0, 16);
            ImGui::BeginGroup();
            if (kGroup("##kGVol", &gvol, 0, 100, "G.Vol"))
                ia->set_global_volume(gvol / 100.0);
            ImGui::EndGroup();

            ImGui::Spacing();
            double bpm = m_state.bpm.load();
            ImGui::TextColored(kDim, "BPM");
            ImGui::SameLine(0, 5);
            ImGui::TextColored(kVal, "%.1f", bpm);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Tempo factor / Pitch factor sliders
            int tempoFactPct = (int)std::round(ia->get_tempo_factor() * 100.0);
            int pitchFactPct = (int)std::round(ia->get_pitch_factor() * 100.0);

            if (ImGui::BeginTable("##playtbl", 2, ImGuiTableFlags_None)) {
                ImGui::TableSetupColumn("##pl", ImGuiTableColumnFlags_WidthFixed, 70.f);
                ImGui::TableSetupColumn("##pc", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(kDim, "Tempo x");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::SliderInt("##tempofact", &tempoFactPct, 25, 400, "%d%%"))
                    ia->set_tempo_factor(tempoFactPct / 100.0);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(kDim, "Pitch x");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::SliderInt("##pitchfact", &pitchFactPct, 25, 400, "%d%%"))
                    ia->set_pitch_factor(pitchFactPct / 100.0);

                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 12.f);

    // ── Render panel ─────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 14, 26, 255));
    if (ImGui::BeginChild("##genrender", {colW, avH}, true)) {
        ImGui::TextColored(kHdr, "RENDER");
        ImGui::Separator();
        ImGui::Spacing();

        if (mod) {
            if (ImGui::BeginTable("##rendertbl", 2, ImGuiTableFlags_None)) {
                ImGui::TableSetupColumn("##rl", ImGuiTableColumnFlags_WidthFixed, 95.f);
                ImGui::TableSetupColumn("##rc", ImGuiTableColumnFlags_WidthStretch);

                // Master Gain
                int masterGain = mod->get_render_param(openmpt::module::RENDER_MASTERGAIN_MILLIBEL);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(kDim, "Master Gain");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                char gainLbl[24]; snprintf(gainLbl, sizeof(gainLbl), "%.1f dB", masterGain / 100.f);
                if (ImGui::SliderInt("##mastergain", &masterGain, -2400, 2400, gainLbl))
                    mod->set_render_param(openmpt::module::RENDER_MASTERGAIN_MILLIBEL, masterGain);

                // Stereo Separation
                int stereoSep = mod->get_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(kDim, "Stereo Sep");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::SliderInt("##stereosep", &stereoSep, 0, 200, "%d%%"))
                    mod->set_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT, stereoSep);

                // Interpolation
                static const char* kInterpNames[] = {"Default", "None (ZOH)", "Linear", "Cubic", "Sinc 8x"};
                static const int   kInterpVals[]  = {0, 1, 2, 4, 8};
                int interp    = mod->get_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH);
                int interpIdx = 0;
                for (int i = 0; i < 5; ++i) if (kInterpVals[i] == interp) { interpIdx = i; break; }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(kDim, "Interpolation");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::Combo("##interp", &interpIdx, kInterpNames, 5))
                    mod->set_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, kInterpVals[interpIdx]);

                // Volume Ramping
                int ramping = mod->get_render_param(openmpt::module::RENDER_VOLUMERAMPING_STRENGTH);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(kDim, "Vol Ramping");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                char rampLbl[16]; snprintf(rampLbl, sizeof(rampLbl), ramping < 0 ? "Default" : "%d", ramping);
                if (ImGui::SliderInt("##ramping", &ramping, -1, 10, rampLbl))
                    mod->set_render_param(openmpt::module::RENDER_VOLUMERAMPING_STRENGTH, ramping);

                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void App::drawSamplesTab()
{
    if (!m_hasModule) {
        ImVec2 av = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos({av.x * 0.5f - 60.f, av.y * 0.5f - 8.f});
        ImGui::TextDisabled("No module loaded.");
        return;
    }
    auto* mod = m_audio.getModule();
    if (!mod) return;
    int nSamp;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        nSamp = m_state.num_samples;
    }
    if (nSamp == 0) {
        ImGui::TextDisabled("No samples in this module.");
        return;
    }
    static openmpt::module_ext* lastMod = nullptr;
    if (mod != lastMod) {
        m_sampleView.setFonts(m_fontMono, m_fontUI);
        m_sampleView.setModule(mod, nSamp);
        m_sampleView.onNeedAudio = [this]() {
            if (!m_hasModule) return;
            m_audio.startPreview();
        };
        lastMod = mod;
    }
    ImVec2 pos   = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_sampleView.render(pos.x, pos.y, avail.x, avail.y);
}

void App::drawInstrumentsTab()
{
    if (!m_hasModule) {
        ImVec2 av = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos({av.x * 0.5f - 60.f, av.y * 0.5f - 8.f});
        ImGui::TextDisabled("No module loaded.");
        return;
    }
    auto* mod = m_audio.getModule();
    if (!mod) return;
    int nInstr;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        nInstr = m_state.num_instruments;
    }
    if (nInstr == 0) {
        ImGui::TextDisabled("Sample-based module \xe2\x80\x94 no instruments.");
        return;
    }
    // Lazy init when module changes
    static openmpt::module_ext* lastMod = nullptr;
    if (mod != lastMod) {
        m_instrView.setFonts(m_fontMono, m_fontUI);
        m_instrView.setModule(mod, nInstr);
        m_instrView.onNeedAudio = [this]() {
            if (!m_hasModule) return;
            m_audio.startPreview();
        };
        lastMod = mod;
    }
    ImVec2 pos   = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_instrView.render(pos.x, pos.y, avail.x, avail.y);
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

void App::saveModule()
{
    if (!m_hasModule || m_loadedPath.empty()) return;
    auto* mod = m_audio.getModule();
    if (!mod) return;
    std::string type = mod->get_metadata("type");
    bb_save_module(mod, m_loadedPath, type);
}

void App::insertPattern()
{
    if (!m_hasModule) return;
    auto* mod = m_audio.getModule();
    if (!mod) return;
    int idx = bb_insert_pattern(mod, 64);
    if (idx >= 0) {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        // Ensure pattern_names is large enough for the new index
        if (idx >= (int)m_state.pattern_names.size())
            m_state.pattern_names.resize(idx + 1);
        m_state.num_patterns = std::max(m_state.num_patterns, idx + 1);
    }
}

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
    m_instrView.clearModule();
    m_sampleView.clearModule();
    m_state.reset();
    m_hasModule  = false;
    m_loadedPath.clear();
}

void App::startPlayback()
{
    if (!m_hasModule || m_audio.isRunning()) return;
    m_audio.stopPreview();
    m_audio.setPaused(false);
    m_audio.start();
}

void App::stopPlayback()
{
    if (!m_audio.isRunning()) return;
    m_audio.stopPreview();
    m_audio.stop();
    m_audio.join();
}

void App::togglePlayPause()
{
    if (!m_hasModule) return;
    if (!m_audio.isRunning()) {
        startPlayback();
    } else if (m_audio.isPaused()) {
        m_audio.stopPreview();
        m_audio.setPaused(false);
    } else {
        m_audio.setPaused(true);
    }
}

void App::playFromStart()
{
    if (!m_hasModule) return;
    m_audio.stopPreview();
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
