#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "SettingsView.h"
#include "Knob.h"
#include "imgui.h"
#include "Theme.h"

void SettingsView::open(const Settings& current,
                        std::vector<AudioDeviceInfo> audioDevs,
                        std::vector<MidiSourceInfo>  midiDevs)
{
    m_pending  = current;
    m_audioDevs = std::move(audioDevs);
    m_midiDevs  = std::move(midiDevs);

    m_audioDevSel = 0;
    for (int i = 0; i < (int)m_audioDevs.size(); ++i)
        if (m_audioDevs[i].index == current.audioDeviceIndex) { m_audioDevSel = i + 1; break; }

    m_midiDevSel = 0;
    for (int i = 0; i < (int)m_midiDevs.size(); ++i)
        if (m_midiDevs[i].index == current.midiInputIndex) { m_midiDevSel = i + 1; break; }

    m_open        = true;
    m_shouldOpen  = true;
    m_learnTarget = -1;
    m_winPosX     = -1.f;  // reset to center
    m_winPosY     = -1.f;
}

void SettingsView::draw()
{
    if (!m_open && !m_shouldOpen) return;

    if (m_shouldOpen) {
        ImGui::OpenPopup("Settings");
        m_shouldOpen = false;
    }

    // Centre on first open; preserve position across frames so dragging works
    if (m_winPosX < 0.f) {
        ImVec2 c = ImGui::GetMainViewport()->GetCenter();
        m_winPosX = c.x - 250.f;
        m_winPosY = c.y - 170.f;
    }

    constexpr float kW = 500.f, kH = 340.f, kTitleH = 22.f;

    ImGui::SetNextWindowPos({m_winPosX, m_winPosY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({kW, kH}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,          kClrModalBg);

    if (ImGui::BeginPopupModal("Settings", &m_open,
            ImGuiWindowFlags_NoTitleBar        |
            ImGuiWindowFlags_NoResize          |
            ImGuiWindowFlags_NoScrollbar       |
            ImGuiWindowFlags_NoScrollWithMouse)) {

        ImVec2 p0  = ImGui::GetWindowPos();
        ImVec2 p1  = {p0.x + kW, p0.y + kH};
        ImDrawList* wdl = ImGui::GetWindowDrawList();

        // ── Shadow (window draw list + full-screen clip so it bleeds outside) ──
        wdl->PushClipRectFullScreen();
        for (int i = 24; i >= 1; i--) {
            float f = float(i) / 24.f;
            float r = 40.f * f;
            auto  a = (uint8_t)(10.f * (1.f - f) * (1.f - f));
            if (!a) continue;
            wdl->AddRectFilled({p0.x - r, p0.y + 10.f - r},
                               {p1.x + r, p1.y + 2.f + r},
                               IM_COL32(0, 0, 0, a), 6.f + r * 0.5f);
        }
        wdl->PopClipRect();

        // ── Gradient background ──────────────────────────────────────────────
        wdl->AddRectFilledMultiColor(p0, p1,
            kClrModalGradTop, kClrModalGradTop,
            kClrModalGradBot, kClrModalGradBot);

        // ── Custom title bar ─────────────────────────────────────────────────
        wdl->AddRectFilled(p0, {p1.x, p0.y + kTitleH}, kClrBgTitle);
        {
            const char* title = "Settings";
            ImVec2 tsz = ImGui::CalcTextSize(title);
            wdl->AddText({p0.x + (kW - tsz.x) * 0.5f, p0.y + (kTitleH - tsz.y) * 0.5f},
                         kClrTextTitle, title);
        }

        // Drag handle over title bar (excludes X button area)
        ImGui::SetCursorPos({0.f, 0.f});
        ImGui::InvisibleButton("##drag", {kW - kTitleH, kTitleH});
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            m_winPosX += d.x;
            m_winPosY += d.y;
        }

        // X close button
        ImGui::SetCursorPos({kW - kTitleH, 0.f});
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kClrModalClose);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kClrModalCloseAct);
        ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(180, 180, 200, 220));
        if (ImGui::Button("\xc3\x97##close", {kTitleH, kTitleH})) {
            m_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(4);

        // ── Tabs ─────────────────────────────────────────────────────────────
        ImGui::SetCursorPos({0.f, kTitleH});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,     kTabFramePad);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, kTabInnerSpacing);
        ImGui::PushStyleColor(ImGuiCol_Tab,        kClrTabBg);
        ImGui::PushStyleColor(ImGuiCol_TabHovered, kClrTabHover);
        if (ImGui::BeginTabBar("##settabs")) {
            const char* tabs[] = {"General", "Audio", "MIDI", "Editor"};
            using DrawFn = void (SettingsView::*)();
            DrawFn fns[] = { &SettingsView::drawGeneralTab, &SettingsView::drawAudioTab,
                             &SettingsView::drawMidiTab,    &SettingsView::drawEditorTab };
            for (int i = 0; i < 4; ++i) {
                ImGui::PushStyleColor(ImGuiCol_Text, (i == m_activeTab)
                    ? kClrTextBright
                    : kClrTextDim);
                if (ImGui::BeginTabItem(tabs[i])) {
                    m_activeTab = i;
                    (this->*fns[i])();
                    ImGui::EndTabItem();
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndTabBar();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(2);

    if (!ImGui::IsPopupOpen("Settings"))
        m_open = false;
}

void SettingsView::drawAudioTab()
{
    ImGui::Spacing();

    // ── Output device ──────────────────────────────────────────────────────
    ImGui::TextUnformatted("Output Device");
    ImGui::SetNextItemWidth(-1.f);

    const char* devLabel = (m_audioDevSel == 0)
        ? "(Default)"
        : m_audioDevs[m_audioDevSel - 1].name.c_str();

    if (ImGui::BeginCombo("##audiodev", devLabel)) {
        if (ImGui::Selectable("(Default)", m_audioDevSel == 0)) {
            m_audioDevSel = 0;
            m_pending.audioDeviceIndex = -1;
            if (onApply) onApply(m_pending);
        }
        for (int i = 0; i < (int)m_audioDevs.size(); ++i) {
            bool sel = (m_audioDevSel == i + 1);
            if (ImGui::Selectable(m_audioDevs[i].name.c_str(), sel)) {
                m_audioDevSel = i + 1;
                m_pending.audioDeviceIndex = m_audioDevs[i].index;
                if (onApply) onApply(m_pending);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // ── Sample rate ────────────────────────────────────────────────────────
    ImGui::TextUnformatted("Sample Rate");
    ImGui::SetNextItemWidth(-1.f);

    static const int        kRates[]      = {44100, 48000, 96000};
    static const char*      kRateLabels[] = {"44100 Hz", "48000 Hz", "96000 Hz"};
    int rateIdx = 1;
    for (int i = 0; i < 3; ++i)
        if (kRates[i] == m_pending.sampleRate) { rateIdx = i; break; }

    if (ImGui::BeginCombo("##srate", kRateLabels[rateIdx])) {
        for (int i = 0; i < 3; ++i) {
            if (ImGui::Selectable(kRateLabels[i], rateIdx == i)) {
                m_pending.sampleRate = kRates[i];
                rateIdx = i;
                if (onApply) onApply(m_pending);
            }
            if (rateIdx == i) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Audio changes take effect when playback is next started.");
}

bool SettingsView::isLearning() const { return m_learnTarget >= 0; }

void SettingsView::feedMidiCC(int cc)
{
    if (m_learnTarget < 0) return;
    int* targets[] = {
        &m_pending.midiCC.oct,
        &m_pending.midiCC.bpm,
        &m_pending.midiCC.spd,
        &m_pending.midiCC.rpb,
        &m_pending.midiCC.vol,
    };
    *targets[m_learnTarget] = cc;
    m_learnTarget = -1;
    if (onApply) onApply(m_pending);
}

void SettingsView::drawMidiTab()
{
    ImGui::Spacing();

    // ── Device picker ──────────────────────────────────────────────────────
    ImGui::TextUnformatted("MIDI Input");
    ImGui::SetNextItemWidth(-1.f);

    const char* midiLabel = (m_midiDevSel == 0)
        ? "(None)"
        : m_midiDevs[m_midiDevSel - 1].name.c_str();

    if (ImGui::BeginCombo("##mididev", midiLabel)) {
        if (ImGui::Selectable("(None)", m_midiDevSel == 0)) {
            m_midiDevSel = 0;
            m_pending.midiInputIndex = -1;
            if (onMidiSourceChange) onMidiSourceChange(-1);
            if (onApply) onApply(m_pending);
        }
        for (int i = 0; i < (int)m_midiDevs.size(); ++i) {
            bool sel = (m_midiDevSel == i + 1);
            if (ImGui::Selectable(m_midiDevs[i].name.c_str(), sel)) {
                m_midiDevSel = i + 1;
                m_pending.midiInputIndex = m_midiDevs[i].index;
                if (onMidiSourceChange) onMidiSourceChange(m_midiDevs[i].index);
                if (onApply) onApply(m_pending);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (m_midiDevs.empty())
        ImGui::TextDisabled("No MIDI sources found.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── CC assignment table ────────────────────────────────────────────────
    ImGui::TextUnformatted("Knob Assignments");
    ImGui::Spacing();

    struct Row { const char* name; int* cc; int target; };
    Row rows[] = {
        {"Octave", &m_pending.midiCC.oct, 0},
        {"BPM",    &m_pending.midiCC.bpm, 1},
        {"Speed",  &m_pending.midiCC.spd, 2},
        {"RPB",    &m_pending.midiCC.rpb, 3},
        {"Volume", &m_pending.midiCC.vol, 4},
    };

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {6.f, 4.f});
    if (ImGui::BeginTable("##ccmap", 3, ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("Knob",   ImGuiTableColumnFlags_WidthFixed,   80.f);
        ImGui::TableSetupColumn("CC",     ImGuiTableColumnFlags_WidthFixed,   60.f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto& row : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name);

            ImGui::TableSetColumnIndex(1);
            if (*row.cc >= 0) {
                ImGui::Text("%d", *row.cc);
            } else {
                ImGui::TextDisabled("\xe2\x80\x93");  // –
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(row.target);

            bool learning = (m_learnTarget == row.target);
            if (learning) {
                ImGui::PushStyleColor(ImGuiCol_Button,        kClrRed);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kClrRedHover);
            }
            if (ImGui::SmallButton(learning ? "Waiting..." : "Learn")) {
                m_learnTarget = learning ? -1 : row.target;
            }
            if (learning) ImGui::PopStyleColor(2);

            if (*row.cc >= 0) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear")) {
                    *row.cc = -1;
                    if (m_learnTarget == row.target) m_learnTarget = -1;
                    if (onApply) onApply(m_pending);
                }
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();

    if (m_learnTarget >= 0) {
        ImGui::Spacing();
        ImGui::TextColored({1.f, 0.6f, 0.3f, 1.f}, "Move a knob on your controller...");
    }
}

void SettingsView::drawEditorTab()
{
    ImGui::Spacing();
    if (ImGui::Checkbox("Follow playback cursor", &m_pending.followPlayback))
        if (onApply) onApply(m_pending);
    if (ImGui::Checkbox("Wrap cursor at pattern end", &m_pending.wrapCursor))
        if (onApply) onApply(m_pending);
}

void SettingsView::drawGeneralTab()
{
    ImGui::Spacing();
    ImGui::TextUnformatted("Glow");
    ImGui::Spacing();

    struct { const char* label; int* val; } knobs[] = {
        {"Logo",    &m_pending.glowLogo},
        {"VU",      &m_pending.glowVU},
        {"Pattern", &m_pending.glowPattern},
        {"Knobs",   &m_pending.glowKnobs},
    };

    bool changed = false;
    for (int i = 0; i < 4; ++i) {
        if (i > 0) ImGui::SameLine(0, 20);
        changed |= KnobWidget(knobs[i].label, knobs[i].val, 0, 10,
                              nullptr, false, false,
                              knobs[i].label, KnobLabelPos::Top);
    }
    if (changed && onApply) onApply(m_pending);
}
