#include "App.h"
#include "FileDialog.h"
#include "imgui.h"
#include <libopenmpt/libopenmpt_ext.hpp>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

static bool isModuleFile(const fs::path& p)
{
    std::string ext = p.extension().string();
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    static const std::unordered_set<std::string> kExts = {
        ".mod", ".s3m", ".xm",  ".it",  ".mptm", ".669", ".amf", ".ams",
        ".dbm", ".digi",".dmf", ".dsm", ".dtm",  ".far", ".gdm", ".imf",
        ".j2b", ".mdl", ".med", ".mt2", ".mtm",  ".okt", ".plm", ".ptm",
        ".sfx", ".stm", ".stp", ".ult", ".wow",  ".mo3", ".umx"
    };
    return kExts.count(ext) > 0;
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────

App::App() : m_audio(m_state), m_tracker(m_state)
{
    // ── Toolbar callbacks ──────────────────────────────────────────────────
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

    // ── Browser ───────────────────────────────────────────────────────────
    const char* home = std::getenv("HOME");
    browserNavigate(home ? fs::path(home) : fs::current_path());
}

App::~App()
{
    m_audio.stop();
    m_audio.join();
}

// ─── Public API ───────────────────────────────────────────────────────────────

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
        // Navigate sidebar to the file's directory
        fs::path fp(path);
        if (fp.has_parent_path()) browserNavigate(fp.parent_path());
        startPlayback();
    }
}

// ─── Render ───────────────────────────────────────────────────────────────────

void App::render(int winW, int winH)
{
    const float w = (float)winW;
    const float h = (float)winH;

    syncToolbarState();

    // Toolbar occupies the top band (full width)
    float contentY = DrawToolbar(m_toolbarState, m_knobEdit,
                                 m_fontUI, 0.f, 0.f, w, kToolbarH);

    ImGui::PushFont(m_fontUI);

    // ── Sidebar ───────────────────────────────────────────────────────────
    drawSidebar(0.f, contentY, m_sidebarW, h - contentY);

    // ── Splitter (drag handle) ────────────────────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(35, 35, 58, 255));
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
    ImGui::PushStyleColor(ImGuiCol_WindowBg,  IM_COL32(24, 24, 40, 255));
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
}

// ─── Browser ──────────────────────────────────────────────────────────────────

void App::browserNavigate(const fs::path& path)
{
    std::error_code ec;
    if (!fs::is_directory(path, ec)) return;

    m_browserPath = path;
    m_browserEntries.clear();

    for (auto& e : fs::directory_iterator(path, ec)) {
        if (ec) break;
        auto name = e.path().filename().string();
        if (name.empty() || name[0] == '.') continue;  // skip hidden
        if (e.is_directory(ec) || (e.is_regular_file(ec) && isModuleFile(e.path())))
            m_browserEntries.push_back(e);
    }

    // Sort: directories first, then files; each group alphabetically
    std::sort(m_browserEntries.begin(), m_browserEntries.end(),
              [](const fs::directory_entry& a, const fs::directory_entry& b) {
                  std::error_code ec;
                  bool aDir = a.is_directory(ec), bDir = b.is_directory(ec);
                  if (aDir != bDir) return aDir > bDir;
                  return a.path().filename() < b.path().filename();
              });
}

void App::drawSidebar(float x, float y, float w, float h)
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg,          IM_COL32(18, 18, 30, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,       IM_COL32(18, 18, 30, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,     IM_COL32(55, 55, 85, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(75, 75, 110, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  IM_COL32(95, 95, 140, 255));
    ImGui::PushStyleColor(ImGuiCol_Header,            IM_COL32(45, 45, 75, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,     IM_COL32(60, 60, 95, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,      IM_COL32(75, 75, 115, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(6.f, 4.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(4.f, 2.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);
    ImGui::Begin("##browser", nullptr,
        ImGuiWindowFlags_NoTitleBar    |
        ImGuiWindowFlags_NoResize      |
        ImGuiWindowFlags_NoCollapse    |
        ImGuiWindowFlags_NoMove        |
        ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(8);

    // ── Header ────────────────────────────────────────────────────────────
    ImGui::TextColored({0.6f, 0.65f, 0.9f, 1.f}, "Browser");
    ImGui::SameLine(w - 24.f);
    if (ImGui::SmallButton(" ^ ") && m_browserPath.has_parent_path())
        browserNavigate(m_browserPath.parent_path());

    // Current directory name (dimmed)
    {
        std::string name = m_browserPath.filename().string();
        if (name.empty()) name = m_browserPath.string();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.65f, 1.f));
        // Truncate display to fit width
        ImGui::SetNextItemWidth(w - 12.f);
        ImGui::TextUnformatted(name.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // ── File / directory list ─────────────────────────────────────────────
    for (int i = 0; i < (int)m_browserEntries.size(); ++i) {
        const auto& e   = m_browserEntries[i];
        std::error_code ec;
        bool isDir      = e.is_directory(ec);
        std::string name = e.path().filename().string();

        // Directories: blue-white; files: warm yellow
        ImVec4 col = isDir
            ? ImVec4(0.70f, 0.78f, 1.00f, 1.f)
            : ImVec4(0.88f, 0.88f, 0.70f, 1.f);

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        bool clicked = ImGui::Selectable(name.c_str(), false,
                                         ImGuiSelectableFlags_None, {w - 18.f, 0.f});
        ImGui::PopStyleColor();
        ImGui::PopID();

        if (clicked) {
            if (isDir) browserNavigate(e.path());
            else       openFile(e.path().string());
        }

        // Tooltip showing full path on hover
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", e.path().string().c_str());
    }

    ImGui::End();
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void App::syncToolbarState()
{
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
