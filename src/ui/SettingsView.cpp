#include "SettingsView.h"
#include "Knob.h"
#include "TabBar.h"
#include "imgui.h"
#include "Theme.h"

void SettingsView::open(const Settings& current,
                        std::vector<AudioDeviceInfo> audioDevs,
                        std::vector<MidiSourceInfo>  midiDevs)
{
    m_pending   = current;
    m_audioDevs = std::move(audioDevs);
    m_midiDevs  = std::move(midiDevs);

    m_audioDevSel = 0;
    for (int i = 0; i < (int)m_audioDevs.size(); ++i)
        if (m_audioDevs[i].index == current.audioDeviceIndex) { m_audioDevSel = i + 1; break; }

    m_midiDevSel = 0;
    for (int i = 0; i < (int)m_midiDevs.size(); ++i)
        if (m_midiDevs[i].index == current.midiInputIndex) { m_midiDevSel = i + 1; break; }

    m_learnTarget = -1;
    m_popup.open();
}

void SettingsView::draw()
{
    if (!m_popup.begin("Settings", 500.f, 340.f)) return;

    if (BeginStyledTabBar("##settabs")) {
        const char* tabs[] = {"General", "Audio", "MIDI", "Editor"};
        using DrawFn = void (SettingsView::*)();
        DrawFn fns[] = { &SettingsView::drawGeneralTab, &SettingsView::drawAudioTab,
                         &SettingsView::drawMidiTab,    &SettingsView::drawEditorTab };
        for (int i = 0; i < 4; ++i) {
            if (StyledTabItem(tabs[i])) {
                (this->*fns[i])();
                ImGui::EndTabItem();
            }
        }
        EndStyledTabBar();
    }

    m_popup.end();
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
    if (ImGui::Checkbox("Continuous pattern scroll", &m_pending.continuousScroll))
        if (onApply) onApply(m_pending);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Playback");
    ImGui::Spacing();
    if (ImGui::Checkbox("FT2 accurate volume ramping", &m_pending.ft2VolumeRamping))
        if (onApply) onApply(m_pending);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enables 5ms note attack ramps as in original FastTracker 2.\nDisabled matches OpenMPT desktop default (punchier attacks).");
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
