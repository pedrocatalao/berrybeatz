#include "App.h"
#include "CocoaHelpers.h"
#include "Logo.h"
#include "MidiDevices.h"
#include "OpenMPTInternal.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <libopenmpt/libopenmpt_ext.hpp>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <unordered_set>

// ─── Constructor / Destructor ─────────────────────────────────────────────────

App::App() : m_audio(m_state), m_tracker(m_state), m_generalView(m_state)
{
    // ── Toolbar callbacks ──────────────────────────────────────────────────
    // ── Window control callbacks ───────────────────────────────────────────
    m_toolbarState.onWindowClose      = [this]() { glfwSetWindowShouldClose(m_glfwWindow, 1); };
    m_toolbarState.onWindowMinimize   = [this]() { glfwIconifyWindow(m_glfwWindow); };
    m_toolbarState.onWindowMaximize   = [this]() { m_chrome.toggleMaximize(); };
    m_toolbarState.onWindowFullscreen = [this]() { m_chrome.toggleFullscreen(); };

    m_toolbarState.onSetup = [this]() {
        m_settingsView.open(m_settings, m_audio.getDevices(), enumerateMidiSources());
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

    // ── Settings ──────────────────────────────────────────────────────────
    m_settings.load(Settings::defaultPath());
    m_audio.setAudioConfig(m_settings.audioDeviceIndex, m_settings.sampleRate);
    m_midi.setSource(m_settings.midiInputIndex);

    m_settingsView.onMidiSourceChange = [this](int idx) {
        m_midi.setSource(idx);
    };

    m_settingsView.onApply = [this](const Settings& s) {
        bool audioChanged = (s.audioDeviceIndex != m_settings.audioDeviceIndex ||
                             s.sampleRate       != m_settings.sampleRate);
        bool midiChanged  = (s.midiInputIndex   != m_settings.midiInputIndex);
        m_settings = s;
        m_settings.save(Settings::defaultPath());
        m_audio.setAudioConfig(s.audioDeviceIndex, s.sampleRate);
        if (audioChanged && m_audio.isRunning()) {
            m_audio.stop();
            m_audio.join();
            m_audio.start();
        }
        if (midiChanged)
            m_midi.setSource(s.midiInputIndex);
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
    m_tracker.onLoopToggled = [this](bool loop) { m_audio.setLoopPattern(loop); };

    m_sidebar.onLeafActivated = [this](int tab, int idx) {
        m_pendingTab = tab;
        if (tab == 3) {
            m_instrView.setSelection(idx + 1);
        } else if (tab == 2) {
            m_sampleView.setSelection(idx + 1);
        } else if (tab == 1) {
            auto* mod = m_audio.getModule();
            if (mod) {
                int numOrd;
                { std::lock_guard<std::mutex> lk(m_state.meta_mutex); numOrd = m_state.num_orders; }
                int order = -1;
                for (int o = 0; o < numOrd; ++o)
                    if (mod->get_order_pattern(o) == idx) { order = o; break; }
                if (order >= 0)
                    m_audio.playPatternFrom(order, 0, m_tracker.getLoopPattern());
            }
        }
    };
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
    m_chrome.setWindow(w);
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
        auto* mod = m_audio.getModule();
        m_tracker.setModule(mod);
        m_generalView.setModule(mod);

        int nSamp, nInstr;
        {
            std::lock_guard<std::mutex> lk(m_state.meta_mutex);
            nSamp  = m_state.num_samples;
            nInstr = m_state.num_instruments;
        }

        m_sampleView.setFonts(m_fontMono, m_fontUI);
        m_sampleView.setModule(mod, nSamp);
        m_sampleView.onNeedAudio = [this]() {
            if (!m_hasModule) return;
            m_audio.startPreview();
        };

        m_instrView.setFonts(m_fontMono, m_fontUI);
        m_instrView.setModule(mod, nInstr);
        m_instrView.onNeedAudio = [this]() {
            if (!m_hasModule) return;
            m_audio.startPreview();
        };

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
    {
        SidebarData sd;
        int numOrd = 0;
        {
            std::lock_guard<std::mutex> lk(m_state.meta_mutex);
            sd.songTitle   = m_state.title;
            sd.instrNames  = m_state.instrument_names;
            sd.sampleNames = m_state.sample_names;
            sd.patNames    = m_state.pattern_names;
            numOrd         = m_state.num_orders;
        }
        if (auto* mod = m_audio.getModule())
            for (int o = 0; o < numOrd; ++o) {
                int p = mod->get_order_pattern(o);
                if (p >= 0) sd.usedPats.insert(p);
            }
        m_sidebar.render(0.f, contentY, m_sidebarW, h - contentY, m_hasModule, sd);
    }

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
            ImGuiTabItemFlags flags = (m_pendingTab == i)
                                      ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(tabs[i], nullptr, flags)) {
                m_activeTab = i;
                ImGui::EndTabItem();
            }
            ImGui::PopStyleColor();
        }
        m_tabsInited = true;
        m_pendingTab = -1;
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

    // Use pending tab if a sidebar click fired this frame, otherwise the confirmed active tab
    const int displayTab = (m_pendingTab >= 0) ? m_pendingTab : m_activeTab;

    if (displayTab == 1) {
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

        switch (displayTab) {
        case 0: { ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 avail = ImGui::GetContentRegionAvail(); m_generalView.render(pos.x, pos.y, avail.x, avail.y); } break;
        case 2: drawSamplesTab();      break;
        case 3: drawInstrumentsTab();  break;
        case 4: drawCommentsTab();     break;
        }

        ImGui::End();
    }

    // ── MIDI ──────────────────────────────────────────────────────────────
    {
        auto events = m_midi.poll();
        if (m_settingsView.isLearning()) {
            if (!events.empty())
                m_settingsView.feedMidiCC(events.front().cc);
        } else {
            for (auto& e : events)
                dispatchMidiCC(e.cc, e.value);
        }

        for (auto& n : m_midi.pollNotes()) {
            if (n.vel > 0) {
                if (m_activeTab == 2) m_sampleView.midiNoteOn(n.note, n.vel);
                if (m_activeTab == 3) m_instrView.midiNoteOn(n.note, n.vel);
            } else {
                if (m_activeTab == 2) m_sampleView.midiNoteOff(n.note);
                if (m_activeTab == 3) m_instrView.midiNoteOff(n.note);
            }
        }
        int pb = m_midi.getPitchBend();
        if (m_activeTab == 2) m_sampleView.midiPitchBend(pb);
        if (m_activeTab == 3) m_instrView.midiPitchBend(pb);
    }

    m_settingsView.draw();

    ImGui::PopFont();
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


void App::drawSamplesTab()
{
    if (!m_hasModule) {
        ImVec2 av = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos({av.x * 0.5f - 60.f, av.y * 0.5f - 8.f});
        ImGui::TextDisabled("No module loaded.");
        return;
    }
    m_sampleView.setBaseOctave(m_toolbarState.octave);
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
    m_instrView.setBaseOctave(m_toolbarState.octave);
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
    m_generalView.clearModule();
    m_instrView.clearModule();
    m_sampleView.clearModule();
    m_sidebar.resetState();
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

void App::dispatchMidiCC(int cc, int value)
{
    // Map 0-127 linearly onto the knob's range
    auto scale = [value](int lo, int hi) {
        return lo + (int)std::round((double)value / 127.0 * (hi - lo));
    };
    const auto& m = m_settings.midiCC;
    if (m.oct >= 0 && cc == m.oct) {
        m_toolbarState.octave = scale(0, 8);
        if (m_toolbarState.onOctaveChange) m_toolbarState.onOctaveChange(m_toolbarState.octave);
    }
    if (m.bpm >= 0 && cc == m.bpm) {
        m_toolbarState.tempo = scale(32, 512);
        if (m_toolbarState.onTempoChange) m_toolbarState.onTempoChange(m_toolbarState.tempo);
    }
    if (m.spd >= 0 && cc == m.spd) {
        m_toolbarState.speed = scale(1, 32);
        if (m_toolbarState.onSpeedChange) m_toolbarState.onSpeedChange(m_toolbarState.speed);
    }
    if (m.rpb >= 0 && cc == m.rpb) {
        m_toolbarState.rpb = scale(1, 32);
        if (m_toolbarState.onRPBChange) m_toolbarState.onRPBChange(m_toolbarState.rpb);
    }
    if (m.vol >= 0 && cc == m.vol) {
        m_toolbarState.globalVol = scale(0, 256);
        if (m_toolbarState.onGlobalVolChange) m_toolbarState.onGlobalVolChange(m_toolbarState.globalVol);
    }
}

void App::seekTo(float frac)
{
    double dur = m_state.duration_seconds.load();
    m_audio.seekSeconds(frac * dur);
    if (!m_audio.isRunning() && m_hasModule)
        startPlayback();
}
