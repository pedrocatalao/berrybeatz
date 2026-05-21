#include "SettingsView.h"
#include "imgui.h"

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

    m_open       = true;
    m_shouldOpen = true;
}

void SettingsView::draw()
{
    if (!m_open && !m_shouldOpen) return;

    if (m_shouldOpen) {
        ImGui::OpenPopup("Settings");
        m_shouldOpen = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({500.f, 340.f}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,          IM_COL32(0, 0, 0, 0));

    if (ImGui::BeginPopupModal("Settings", &m_open,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {

        // Gradient background matching toolbar: dark navy top → slightly lighter bottom
        {
            ImVec2 p0 = ImGui::GetWindowPos();
            ImVec2 p1 = {p0.x + ImGui::GetWindowWidth(), p0.y + ImGui::GetWindowHeight()};
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                p0, p1,
                IM_COL32( 0,  0, 20, 255),   // top-left
                IM_COL32( 0,  0, 20, 255),   // top-right
                IM_COL32(20, 20, 40, 255),   // bottom-right
                IM_COL32(20, 20, 40, 255));  // bottom-left
        }

        if (ImGui::BeginTabBar("##settabs")) {
            if (ImGui::BeginTabItem("Audio"))  { drawAudioTab();  ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("MIDI"))   { drawMidiTab();   ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Editor")) { drawEditorTab(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

        // ── Buttons ────────────────────────────────────────────────────────
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 34.f);
        ImGui::Separator();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);

        if (ImGui::Button("Apply", {80.f, 0.f})) {
            if (onApply) onApply(m_pending);
            m_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {80.f, 0.f})) {
            m_open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(2); // ModalWindowDimBg + PopupBg

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
        }
        for (int i = 0; i < (int)m_audioDevs.size(); ++i) {
            bool sel = (m_audioDevSel == i + 1);
            if (ImGui::Selectable(m_audioDevs[i].name.c_str(), sel)) {
                m_audioDevSel = i + 1;
                m_pending.audioDeviceIndex = m_audioDevs[i].index;
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
            }
            if (rateIdx == i) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Changes take effect when playback is next started.");
}

void SettingsView::drawMidiTab()
{
    ImGui::Spacing();
    ImGui::TextUnformatted("MIDI Input");
    ImGui::SetNextItemWidth(-1.f);

    const char* midiLabel = (m_midiDevSel == 0)
        ? "(None)"
        : m_midiDevs[m_midiDevSel - 1].name.c_str();

    if (ImGui::BeginCombo("##mididev", midiLabel)) {
        if (ImGui::Selectable("(None)", m_midiDevSel == 0)) {
            m_midiDevSel = 0;
            m_pending.midiInputIndex = -1;
        }
        for (int i = 0; i < (int)m_midiDevs.size(); ++i) {
            bool sel = (m_midiDevSel == i + 1);
            if (ImGui::Selectable(m_midiDevs[i].name.c_str(), sel)) {
                m_midiDevSel = i + 1;
                m_pending.midiInputIndex = m_midiDevs[i].index;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (m_midiDevs.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No MIDI sources found.");
    }
}

void SettingsView::drawEditorTab()
{
    ImGui::Spacing();
    ImGui::Checkbox("Follow playback cursor", &m_pending.followPlayback);
    ImGui::Checkbox("Wrap cursor at pattern end", &m_pending.wrapCursor);
}
