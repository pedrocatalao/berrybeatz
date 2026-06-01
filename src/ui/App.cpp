#include "App.h"
#include "Theme.h"
#include "TabBar.h"
#include "RenderSettings.h"
#include "CocoaHelpers.h"
#include "LogoView.h"
#include "MidiDevices.h"
#include "SoundModule.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <unordered_set>

// ─── Constructor / Destructor ─────────────────────────────────────────────────

App::App() : m_controller(m_state), m_audio(m_state), m_tracker(m_state), m_generalView(m_state)
{
    // ── Window control callbacks ───────────────────────────────────────────
    m_toolbarState.onWindowClose      = [this]() { glfwSetWindowShouldClose(m_glfwWindow, 1); };
    m_toolbarState.onWindowMinimize   = [this]() { glfwIconifyWindow(m_glfwWindow); };
    m_toolbarState.onWindowMaximize   = [this]() { m_chrome.toggleMaximize(); };
    m_toolbarState.onWindowFullscreen = [this]() { m_chrome.toggleFullscreen(); };

    m_toolbarState.onSetup = [this]() {
        m_settingsView.open(m_settings, m_audio.getDevices(), enumerateMidiSources());
    };

    m_toolbarState.onNew = [this]() { m_newModuleDialog.open(); };

    m_newModuleDialog.onCreate = [this](int fmt) {
        static constexpr const char* kExts[] = {".btz", ".it", ".xm", ".s3m", ".mod"};
        auto doc = std::make_unique<SongDocument>();
        doc->mod = std::make_unique<SoundModule>();
        if (!doc->mod->newModule()) return;
        std::string name = std::string("untitled") + kExts[fmt];
        doc->path        = name;
        doc->displayName = name;
        m_songs.push_back(std::move(doc));
        switchToSong((int)m_songs.size() - 1);
    };

    m_toolbarState.onOpen = [this]() { openFileDialog(); };
    m_toolbarState.onSave = [this]() { saveModule(); };
    m_toolbarState.onStop      = [this]() { stopPlayback(); };
    m_toolbarState.onPlayPause = [this]() { togglePlayPause(); };
    m_toolbarState.onSeek      = [this](float f) { seekTo(f); };

    m_toolbarState.onOctaveChange    = [this](int v) { m_tracker.setBaseOctave(v); };
    m_toolbarState.onTempoChange     = [this](int v) { m_controller.setTempo(v); };
    m_toolbarState.onSpeedChange     = [this](int v) { m_controller.setSpeed(v); };
    m_toolbarState.onRPBChange       = [](int) {};
    m_toolbarState.onGlobalVolChange = [this](int v) {
        int mx = m_state.gvol_max.load();
        if (mx > 0) m_controller.setGlobalVolume(v / (double)mx);
    };

    // ── AudioEngine callbacks ──────────────────────────────────────────────
    m_audio.onLoadFailed = [](const std::string& msg) {
        fprintf(stderr, "Load failed: %s\n", msg.c_str());
    };
    m_audio.onSongFinished = [this]() { (void)this; };

    // ── View callbacks (stable across song switches) ───────────────────────
    m_sampleView.onNeedAudio = [this]() { if (m_hasModule) m_audio.ensureRunning(); };
    m_sampleView.onEnqueuePlay = [this](int si, int on, float v, float p, int key) {
        AudioEngine::PreviewEvent ev{};
        ev.type = AudioEngine::PreviewEvent::SAMPLE_ON;
        ev.key = (uint8_t)key; ev.idx = si; ev.opnote = on; ev.vol = v; ev.pan = p;
        m_audio.enqueuePreviewEvent(ev);
    };
    m_sampleView.onEnqueueStop = [this](int key) {
        AudioEngine::PreviewEvent ev{};
        ev.type = AudioEngine::PreviewEvent::NOTE_OFF;
        ev.key = (uint8_t)key;
        m_audio.enqueuePreviewEvent(ev);
    };
    m_sampleView.onEnqueuePitch = [this](float pitch) {
        AudioEngine::PreviewEvent ev{};
        ev.type = AudioEngine::PreviewEvent::PITCH_BEND;
        ev.pitch = pitch;
        m_audio.enqueuePreviewEvent(ev);
    };

    m_instrView.onNeedAudio = [this]() { if (m_hasModule) m_audio.ensureRunning(); };
    m_instrView.onEnqueuePlay = [this](int si, int on, float v, float p, int key) {
        AudioEngine::PreviewEvent ev{};
        ev.type = AudioEngine::PreviewEvent::INSTR_ON;
        ev.key = (uint8_t)key; ev.idx = si; ev.opnote = on; ev.vol = v; ev.pan = p;
        m_audio.enqueuePreviewEvent(ev);
    };
    m_instrView.onEnqueueStop = [this](int key) {
        AudioEngine::PreviewEvent ev{};
        ev.type = AudioEngine::PreviewEvent::NOTE_OFF;
        ev.key = (uint8_t)key;
        m_audio.enqueuePreviewEvent(ev);
    };
    m_instrView.onEnqueuePitch = [this](float pitch) {
        AudioEngine::PreviewEvent ev{};
        ev.type = AudioEngine::PreviewEvent::PITCH_BEND;
        ev.pitch = pitch;
        m_audio.enqueuePreviewEvent(ev);
    };

    // ── TrackerView callbacks ──────────────────────────────────────────────
    m_tracker.onOrderSeekRequested = [this](int32_t order) {
        if (m_transport == Transport::Playing) {
            m_audio.seekToOrder(order);
        } else if (m_hasModule) {
            auto* mod = m_audio.getModule();
            if (!mod) return;
            mod->setPositionOrderRow(order, 0);
            m_state.current_order.store(order);
            m_state.current_row.store(0);
            int pat = mod->getOrderPattern(order);
            if (pat >= 0) m_state.current_pattern.store(pat);
            m_tracker.requestNavSnap();
        }
    };
    m_tracker.onPatternPlayFromCursor = [this](int32_t order, int32_t row, bool loop) {
        m_audio.playPatternFrom(order, row, loop);
    };
    m_tracker.onPatternPlayFromStart = [this](int32_t order, bool loop) {
        m_audio.playPatternFrom(order, 0, loop);
    };
    m_tracker.onPatternPlayRow  = [this](int32_t order, int32_t row) { m_audio.playRow(order, row); };
    m_tracker.onStopRequested   = [this]() { stopPlayback(); };
    m_tracker.onInsertPatternRequested = [this]() { insertPattern(); };
    m_tracker.onLoopToggled     = [this](bool loop) { m_audio.setLoopPattern(loop); };

    m_tracker.onNoteEntry         = [this](int note) { m_tracker.inputNote(note); };
    m_tracker.onInstrumentChanged = [this](int instr) { m_audio.copyPreviewInstrument(instr); };
    m_tracker.onKbdNeedAudio = [this]() { if (m_hasModule) m_audio.ensureRunning(); };
    m_tracker.onKbdPlay = [this](int note, int key) {
        AudioEngine::PreviewEvent ev{};
        ev.type   = AudioEngine::PreviewEvent::INSTR_ON;
        ev.key    = (uint8_t)key;
        ev.idx    = std::max(0, m_tracker.getSelectedInstrument() - 1);
        ev.opnote = note - 1;
        ev.vol    = 0.85f;
        ev.pan    = 0.5f;
        m_audio.enqueuePreviewEvent(ev);
    };
    m_tracker.onKbdStop = [this](int key) {
        AudioEngine::PreviewEvent ev{};
        ev.type = AudioEngine::PreviewEvent::NOTE_OFF;
        ev.key  = (uint8_t)key;
        m_audio.enqueuePreviewEvent(ev);
    };

    m_sidebar.onLeafActivated = [this](int tab, int idx) {
        m_pendingTab = tab;
        if (tab == 3) {
            m_instrView.setSelection(idx + 1);
            m_audio.copyPreviewInstrument(idx + 1);
        } else if (tab == 2) {
            m_sampleView.setSelection(idx + 1);
            m_audio.copyPreviewSample(idx + 1);
        } else if (tab == 1) {
            auto* mod = m_audio.getModule();
            if (mod) {
                int numOrd;
                { std::lock_guard<std::mutex> lk(m_state.meta_mutex); numOrd = m_state.num_orders; }
                int order = -1;
                for (int o = 0; o < numOrd; ++o)
                    if (mod->getOrderPattern(o) == idx) { order = o; break; }
                if (order >= 0) {
                    if (m_transport == Transport::Playing)
                        m_audio.playPatternFrom(order, 0, m_tracker.getLoopPattern());
                    else {
                        mod->setPositionOrderRow(order, 0);
                        m_state.current_order.store(order);
                        m_state.current_row.store(0);
                        m_state.current_pattern.store(idx);
                        m_tracker.requestNavSnap();
                    }
                }
            }
        }
    };

    // ── Settings ──────────────────────────────────────────────────────────
    m_settings.load(Settings::defaultPath());
    g_glowLogo    = m_settings.glowLogo    / 5.f;
    g_glowVU      = m_settings.glowVU      / 5.f;
    g_glowPattern = m_settings.glowPattern / 5.f;
    g_glowKnobs   = m_settings.glowKnobs   / 5.f;
    m_tracker.setContinuousScroll(m_settings.continuousScroll);
    m_audio.setFT2VolumeRamping(m_settings.ft2VolumeRamping);
    m_audio.setAudioConfig(m_settings.audioDeviceIndex, m_settings.sampleRate);
    m_midi.setSource(m_settings.midiInputIndex);

    m_settingsView.onMidiSourceChange = [this](int idx) { m_midi.setSource(idx); };

    m_settingsView.onApply = [this](const Settings& s) {
        bool audioChanged = (s.audioDeviceIndex != m_settings.audioDeviceIndex ||
                             s.sampleRate       != m_settings.sampleRate);
        bool midiChanged  = (s.midiInputIndex   != m_settings.midiInputIndex);
        m_settings = s;
        m_settings.save(Settings::defaultPath());
        g_glowLogo    = s.glowLogo    / 5.f;
        g_glowVU      = s.glowVU      / 5.f;
        g_glowPattern = s.glowPattern / 5.f;
        g_glowKnobs   = s.glowKnobs   / 5.f;
        m_tracker.setContinuousScroll(s.continuousScroll);
        m_audio.setFT2VolumeRamping(s.ft2VolumeRamping);
        m_audio.setAudioConfig(s.audioDeviceIndex, s.sampleRate);
        if (audioChanged && m_audio.isRunning()) {
            m_audio.stop();
            m_audio.join();
            m_audio.start();
        }
        if (midiChanged) m_midi.setSource(s.midiInputIndex);
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
    auto doc = std::make_unique<SongDocument>();
    doc->mod = std::make_unique<SoundModule>();
    if (!doc->mod->loadFile(path)) return;

    auto slash = path.rfind('/');
    std::string filename = (slash != std::string::npos) ? path.substr(slash + 1) : path;
    auto dot = filename.rfind('.');
    doc->displayName = filename;
    doc->path = path;

    m_songs.push_back(std::move(doc));
    switchToSong((int)m_songs.size() - 1);
}

// ─── Song management ──────────────────────────────────────────────────────────

void App::switchToSong(int idx)
{
    // Save current song state
    if (m_activeSongIdx >= 0 && m_activeSongIdx < (int)m_songs.size()) {
        auto& cur = m_songs[m_activeSongIdx];
        cur->position = m_state.position_seconds.load();
        if (m_transport == Transport::Playing) {
            m_audio.setPaused(true);
            m_transport = Transport::Paused;
        }
        cur->transport = m_transport;
    }

    m_activeSongIdx = idx;
    auto& doc = m_songs[idx];

    // Hand module to audio engine (stops stream, restarts paused)
    m_audio.setModule(doc->mod.get());

    // Restore position
    if (doc->position > 0.0)
        doc->mod->setPositionSeconds(doc->position);

    m_transport = doc->transport;

    // Wire views to new module
    m_controller.setModule(doc->mod.get());
    m_tracker.setController(&m_controller);
    m_generalView.setController(&m_controller);
    activateViews();

    m_hasModule = true;
    wireNoteRT();
}

void App::activateViews()
{
    int nSamp, nInstr;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        nSamp  = m_state.num_samples;
        nInstr = m_state.num_instruments;
    }

    m_sampleView.setFonts(m_fontMono, m_fontUI);
    m_sampleView.setController(&m_controller, nSamp);
    m_instrView.setFonts(m_fontMono, m_fontUI);
    m_instrView.setController(&m_controller, nInstr);

    m_toolbarState.tempo = (int)std::round(m_state.bpm.load());
    m_toolbarState.speed = (int)m_state.current_speed.load();
    m_toolbarState.rpb   = (int)m_state.rows_per_beat.load();

    m_audio.copyPreviewSample(m_sampleView.getSelection());
    m_audio.copyPreviewInstrument(m_instrView.getSelection());
}

void App::closeSong(int idx)
{
    if (m_songs.size() == 1) {
        deactivateAllSongs();
        m_songs.clear();
        m_activeSongIdx = -1;
        m_hasModule = false;
        return;
    }

    if (idx == m_activeSongIdx) {
        int target = (idx < (int)m_songs.size() - 1) ? idx + 1 : idx - 1;
        switchToSong(target);
    }

    m_songs.erase(m_songs.begin() + idx);
    if (m_activeSongIdx > idx) m_activeSongIdx--;
}

void App::deactivateAllSongs()
{
    m_audio.stop();
    m_audio.join();
    m_controller.clearModule();
    m_tracker.clearModule();
    m_generalView.clearModule();
    m_instrView.clearModule();
    m_sampleView.clearModule();
    m_sidebar.resetState();
    m_state.reset();
    m_transport = Transport::Stopped;
}

// ─── Render ───────────────────────────────────────────────────────────────────

void App::render(int winW, int winH)
{
    const float w = (float)winW;
    const float h = (float)winH;
    const float contentH = h - kSongBarH;  // reserve bottom for song bar

    if (m_chrome.isResizing()) {
        ImGui::GetIO().MouseDown[0]    = false;
        ImGui::GetIO().MouseClicked[0] = false;
    }

    syncToolbarState();

    float contentY = DrawToolbar(m_toolbarState, m_fontUI, 0.f, 0.f, w, kToolbarH);

    ImGui::PushFont(m_fontUI);

    // ── Song bar (bottom strip) ───────────────────────────────────────────
    drawSongBar(w, h);

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
                int p = mod->getOrderPattern(o);
                if (p >= 0) sd.usedPats.insert(p);
            }
        if (g_fontSidebar) ImGui::PushFont(g_fontSidebar);
        m_sidebar.render(0.f, contentY, m_sidebarW, contentH - contentY, m_hasModule, sd, kTabBarH);
        if (g_fontSidebar) ImGui::PopFont();
    }

    // ── Splitter ──────────────────────────────────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::SetNextWindowPos({m_sidebarW, contentY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({kSplitterW, contentH - contentY}, ImGuiCond_Always);
        ImGui::Begin("##splitter", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
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
        if (ImGui::IsItemActive())
            m_sidebarW = std::clamp(m_sidebarW + ImGui::GetIO().MouseDelta.x, 100.f, 500.f);
        ImGui::End();
    }

    const float cx = m_sidebarW + kSplitterW;
    const float cw = w - cx;

    // ── Tab bar ───────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_WindowBg,  kClrBgPanel);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(4.f, 2.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowPos({cx, contentY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({cw, kTabBarH}, ImGuiCond_Always);
    ImGui::Begin("##tabbar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    if (BeginStyledTabBar("##tabs")) {
        const char* tabs[] = {"General", "Patterns", "Samples", "Instruments", "Comments"};
        for (int i = 0; i < 5; ++i) {
            ImGuiTabItemFlags flags = (m_pendingTab == i) ? ImGuiTabItemFlags_SetSelected : 0;
            if (StyledTabItem(tabs[i], nullptr, flags)) {
                m_activeTab = i;
                ImGui::EndTabItem();
            }
        }
        m_tabsInited = true;
        m_pendingTab = -1;
        EndStyledTabBar();
    }
    ImGui::End();

    {
        constexpr float tabBarPadX = 1.f;
        constexpr float tabBarPadY = 2.f;
        ImVec4 c   = ImGui::GetStyle().Colors[ImGuiCol_TabSelected];
        ImU32  col = ImGui::ColorConvertFloat4ToU32(c);
        float  lineY = contentY + kTabBarH - tabBarPadY - 2.f;
        ImGui::GetForegroundDrawList()->AddLine({0.f, lineY}, {cx + tabBarPadX, lineY}, col, 1.f);
    }

    // ── Content area ──────────────────────────────────────────────────────
    float cy = contentY + kTabBarH;
    float ch = contentH - cy;

    const int displayTab = (m_pendingTab >= 0) ? m_pendingTab : m_activeTab.load();

    constexpr float kKbdH = 120.f;
    {
        const float dt     = std::min(ImGui::GetIO().DeltaTime, 1.f / 30.f);
        const float target = (m_hasModule && m_toolbarState.showKeyboard &&
                              (displayTab == 1 || displayTab == 2 || displayTab == 3)) ? 1.f : 0.f;
        const float step = 12.f * dt;
        m_kbdAnimT = (target > m_kbdAnimT) ? std::min(m_kbdAnimT + step, target)
                                           : std::max(m_kbdAnimT - step, target);
    }
    const float visibleH = kKbdH * m_kbdAnimT;
    const float activeH  = ch - visibleH;

    if (displayTab == 1) {
        m_tracker.render(cx, cy, cw, activeH);
    } else {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kClrBgContent);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(12.f, 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::SetNextWindowPos({cx, cy}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({cw, activeH}, ImGuiCond_Always);
        ImGui::Begin("##content", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        switch (displayTab) {
        case 0: { ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 avail = ImGui::GetContentRegionAvail(); m_generalView.render(pos.x, pos.y, avail.x, avail.y); } break;
        case 2: drawSamplesTab();     break;
        case 3: drawInstrumentsTab(); break;
        case 4: drawCommentsTab();    break;
        }
        ImGui::End();
    }

    // ── Shared piano keyboard (tabs: Patterns / Samples / Instruments) ────
    // processInput handles note-key audio preview only when the KB panel is not doing it
    m_tracker.setNoteKeyAudioEnabled(
        !(m_kbdAnimT > 0.f && displayTab == 1 && !m_tracker.isEditMode()));

    if (m_kbdAnimT > 0.f) {
        m_kbState.baseOctave = m_toolbarState.octave;

        constexpr ImGuiWindowFlags kNoDecoFlags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        // Window sized to the visible slice; cursor set to where the full keyboard
        // would start so the bottom portion is what's revealed as visibleH grows.
        ImGui::SetNextWindowPos ({cx, cy + ch - visibleH}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({cw, visibleH},            ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::Begin("##appKbd", nullptr, kNoDecoFlags | ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        bool captureKb = (displayTab == 2 || displayTab == 3) ||
                         (displayTab == 1 && !m_tracker.isEditMode());

        DrawPianoKeyboard("##appkbd", cw, kKbdH, m_kbState, m_fontMono, captureKb,
            [this, displayTab](int note, int key) {
                if (displayTab == 1) {
                    if (m_tracker.onNoteEntry)    m_tracker.onNoteEntry(note);
                    if (m_tracker.onKbdNeedAudio) m_tracker.onKbdNeedAudio();
                    if (m_tracker.onKbdPlay)      m_tracker.onKbdPlay(note, key);
                } else if (displayTab == 2) {
                    if (m_sampleView.onNeedAudio) m_sampleView.onNeedAudio();
                    if (m_sampleView.onEnqueuePlay)
                        m_sampleView.onEnqueuePlay(m_sampleView.getSelection(), note, 0.85f, 0.5f, key);
                } else {
                    if (m_instrView.onNeedAudio) m_instrView.onNeedAudio();
                    if (m_instrView.onEnqueuePlay)
                        m_instrView.onEnqueuePlay(m_instrView.getSelection() - 1, note - 1, 0.85f, 0.5f, key);
                }
            },
            [this, displayTab](int key) {
                if (displayTab == 1) {
                    if (m_tracker.onKbdStop) m_tracker.onKbdStop(key);
                } else if (displayTab == 2) {
                    if (m_sampleView.onEnqueueStop) m_sampleView.onEnqueueStop(key);
                } else {
                    if (m_instrView.onEnqueueStop) m_instrView.onEnqueueStop(key);
                }
            });

        ImGui::End();
    }

    // ── MIDI ──────────────────────────────────────────────────────────────
    {
        auto events = m_midi.poll();
        if (m_settingsView.isLearning()) {
            if (!events.empty()) m_settingsView.feedMidiCC(events.front().cc);
        } else {
            for (auto& e : events) dispatchMidiCC(e.cc, e.value);
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

        // Drain MIDI note-entry for patterns tab (RT→UI handoff)
        int midiEntry = m_midiTrackerNote.exchange(-1);
        if (midiEntry > 0) m_tracker.inputNote(midiEntry);
    }

    m_settingsView.draw();
    m_newModuleDialog.draw();

    ImGui::PopFont();
}

// ─── Song bar ─────────────────────────────────────────────────────────────────

void App::drawSongBar(float w, float h)
{
    ImGui::SetNextWindowPos({0.f, h - kSongBarH}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, kSongBarH}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  {4.f, 3.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    {2.f, 0.f});
    ImGui::Begin("##songbar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
    {
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
            wp, {wp.x + ws.x, wp.y + ws.y},
            kClrStatusGradTop, kClrStatusGradTop,
            kClrStatusGradBot, kClrStatusGradBot);
    }

    bool closed   = false;
    int  closeIdx = -1;

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      mp  = ImGui::GetIO().MousePos;
    const float tabH   = kSongBarH - 6.f;
    const float padX   = 7.f;
    const float xGap   = 6.f;
    const float xW     = ImGui::CalcTextSize("x").x + xGap * 2.f;
    const ImU32 kActBg = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_TabSelected]);

    for (int i = 0; i < (int)m_songs.size(); ++i) {
        if (i > 0) ImGui::SameLine(0, 2.f);

        bool isActive = (i == m_activeSongIdx);
        const std::string& name = m_songs[i]->displayName;
        float textW = ImGui::CalcTextSize(name.c_str()).x;
        float tabW  = padX + textW + xW + padX;

        ImVec2 tMin = ImGui::GetCursorScreenPos();
        ImVec2 tMax = {tMin.x + tabW, tMin.y + tabH};
        ImVec2 xMin = {tMax.x - xW, tMin.y};

        bool xHov = (mp.x >= xMin.x && mp.x < tMax.x && mp.y >= tMin.y && mp.y < tMax.y);

        // Background
        if (isActive)
            dl->AddRectFilled(tMin, tMax, kActBg, kStyleTabRounding);
        else if (ImGui::IsMouseHoveringRect(tMin, tMax))
            dl->AddRectFilled(tMin, tMax, IM_COL32(60, 60, 80, 120), kStyleTabRounding);

        // Name
        float ty = tMin.y + (tabH - ImGui::GetTextLineHeight()) * 0.5f;
        dl->AddText({tMin.x + padX, ty}, IM_COL32(220, 220, 235, 255), name.c_str());

        // X
        if (xHov)
            dl->AddRectFilled(xMin, tMax, kClrRed, kStyleTabRounding, ImDrawFlags_RoundCornersRight);
        ImU32 xCol = xHov ? IM_COL32(255, 220, 220, 255) : IM_COL32(180, 175, 200, 200);
        float xTy  = tMin.y + (tabH - ImGui::GetTextLineHeight()) * 0.5f;
        dl->AddText({xMin.x + xGap, xTy}, xCol, "x");

        // Hit area
        std::string bid = "##st" + std::to_string(i);
        if (ImGui::InvisibleButton(bid.c_str(), {tabW, tabH})) {
            if (xHov) { closed = true; closeIdx = i; }
            else if (!isActive) switchToSong(i);
        }
    }

    ImGui::End();

    if (closed) closeSong(closeIdx);
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void App::syncToolbarState()
{
    m_toolbarState.logoTex        = m_logoTex;
    m_toolbarState.logoW          = m_logoW;
    m_toolbarState.logoH          = m_logoH;
    m_toolbarState.speakerConeTex = m_speakerConeTex;
    m_toolbarState.speakerConeW   = m_speakerConeW;
    m_toolbarState.speakerConeH   = m_speakerConeH;
    if (m_audio.pollSongEnded()) m_transport = Transport::Stopped;
    m_toolbarState.hasModule  = m_hasModule;
    m_toolbarState.transport  = m_transport;
    m_toolbarState.vuLeft     = m_state.master_vu_left.load();
    m_toolbarState.vuRight    = m_state.master_vu_right.load();

    if (m_hasModule) {
        double dur = m_state.duration_seconds.load();
        double pos = m_state.position_seconds.load();
        m_toolbarState.seekFrac = (dur > 0.0) ? (float)(pos / dur) : 0.f;
        m_toolbarState.tempo    = (int)std::round(m_state.bpm.load());
        m_toolbarState.speed    = (int)m_state.current_speed.load();
        m_toolbarState.rpb      = (int)m_state.rows_per_beat.load();

        int gvolMax = m_state.gvol_max.load();
        m_toolbarState.gvolMax  = gvolMax;
        if (auto* mod = m_controller.module())
            m_toolbarState.globalVol = (int)std::round(mod->getGlobalVolume() * gvolMax);
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
    static const void* lastMod = nullptr;
    if (mod != lastMod) { msg = mod->getMessage(); lastMod = mod; }
    ImGui::InputTextMultiline("##msg", msg.data(), msg.size() + 1,
                              {-1.f, -1.f}, ImGuiInputTextFlags_ReadOnly);
}

// ─── Operations ───────────────────────────────────────────────────────────────

void App::saveModule()
{
    if (!m_hasModule || m_activeSongIdx < 0) return;
    auto* mod = m_controller.module();
    if (!mod) return;
    const std::string& path = m_songs[m_activeSongIdx]->path;
    if (path.empty()) return;
    m_controller.save(path, mod->getType());
}

void App::insertPattern()
{
    if (!m_hasModule) return;
    int idx = m_controller.insertPattern(64);
    if (idx >= 0) {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
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

void App::startPlayback()
{
    if (!m_hasModule) return;
    if (m_transport == Transport::Playing) return;
    m_audio.setPaused(false);
    if (!m_audio.isRunning()) m_audio.start();
    m_transport = Transport::Playing;
}

void App::stopPlayback()
{
    if (!m_hasModule) return;
    m_audio.setPaused(true);
    if (auto* mod = m_audio.getModule())
        mod->setPositionSeconds(0.0);
    m_transport = Transport::Stopped;
}

void App::togglePlayPause()
{
    if (!m_hasModule) return;
    if (m_transport == Transport::Playing) {
        m_audio.setPaused(true);
        m_transport = Transport::Paused;
    } else {
        m_audio.setPaused(false);
        if (!m_audio.isRunning()) m_audio.start();
        m_transport = Transport::Playing;
    }
}

void App::playFromStart()
{
    if (!m_hasModule) return;
    m_audio.rewind();
    m_audio.start();
}

void App::dispatchMidiCC(int cc, int value)
{
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
    if (m_transport != Transport::Playing && m_hasModule)
        startPlayback();
}

void App::wireNoteRT()
{
    m_midi.onNoteRT = [this](int note, int vel) {
        int tab = m_activeTab.load();
        if (tab != 1 && tab != 2 && tab != 3) return;

        AudioEngine::PreviewEvent ev{};
        if (vel > 0) {
            if (tab == 2) {
                ev.type   = AudioEngine::PreviewEvent::SAMPLE_ON;
                ev.key    = (uint8_t)note;
                ev.idx    = m_sampleView.getSelection();
                ev.opnote = note + 1;
                ev.vol    = vel / 127.f;
                ev.pan    = 0.5f;
            } else {
                ev.type   = AudioEngine::PreviewEvent::INSTR_ON;
                ev.key    = (uint8_t)note;
                ev.idx    = (tab == 1) ? std::max(0, m_tracker.getSelectedInstrument() - 1)
                                       : m_instrView.getSelection() - 1;
                ev.opnote = note;
                ev.vol    = vel / 127.f;
                ev.pan    = 0.5f;
                if (tab == 1)
                    m_midiTrackerNote.store(note + 1);  // hand off to UI thread for pattern entry
            }
            m_kbState.kbHeld[note] = true;
        } else {
            ev.type = AudioEngine::PreviewEvent::NOTE_OFF;
            ev.key  = (uint8_t)note;
            m_kbState.kbHeld[note] = false;
        }
        m_audio.enqueuePreviewEvent(ev);
    };
}
