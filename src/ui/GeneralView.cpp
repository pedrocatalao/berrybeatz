#include "GeneralView.h"
#include "SoundModule.h"
#include "Knob.h"
#include "imgui.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>

GeneralView::GeneralView(ModuleState& state) : m_state(state) {}

void GeneralView::setController(ModuleController* ctrl) { m_ctrl = ctrl; }
void GeneralView::clearModule()                          { m_ctrl = nullptr; }
void GeneralView::setFonts(ImFont* mono, ImFont* ui)     { m_fontMono = mono; m_fontUI = ui; }

void GeneralView::render(float /*x*/, float /*y*/, float /*w*/, float /*h*/)
{
    auto* mod = m_ctrl ? m_ctrl->module() : nullptr;
    if (!mod) {
        ImVec2 av = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos({av.x * 0.5f - 50.f, av.y * 0.5f - 8.f});
        ImGui::TextDisabled("No module loaded.");
        return;
    }

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
        int orders   = (int)m_state.num_orders.load();
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

        int tempo   = (int)std::round(mod->getCurrentTempo2());
        int speed   = (int)mod->getCurrentSpeed();
        int gvolMax = m_state.gvol_max.load();
        int gvol    = (gvolMax > 0) ? (int)std::round(mod->getGlobalVolume() * gvolMax) : 0;

        if (KnobWidget("##kTempo", &tempo, 32, 512, nullptr, false, false, "Tempo"))
            m_ctrl->setTempo(tempo);
        ImGui::SameLine(0, 16);
        if (KnobWidget("##kSpeed", &speed, 1, 32, nullptr, false, false, "Speed"))
            m_ctrl->setSpeed(speed);
        ImGui::SameLine(0, 16);
        if (gvolMax > 0) {
            if (KnobWidget("##kVol", &gvol, 0, gvolMax, nullptr, false, false, "Vol"))
                m_ctrl->setGlobalVolume(gvol / (double)gvolMax);
        }

        ImGui::Spacing();
        double bpm = m_state.bpm.load();
        ImGui::TextColored(kDim, "BPM");
        ImGui::SameLine(0, 5);
        ImGui::TextColored(kVal, "%.1f", bpm);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        int tempoFactPct = (int)std::round(mod->getTempoFactor() * 100.0);
        int pitchFactPct = (int)std::round(mod->getPitchFactor() * 100.0);

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
                m_ctrl->setTempoFactor(tempoFactPct / 100.0);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kDim, "Pitch x");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::SliderInt("##pitchfact", &pitchFactPct, 25, 400, "%d%%"))
                m_ctrl->setPitchFactor(pitchFactPct / 100.0);

            ImGui::EndTable();
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

        if (ImGui::BeginTable("##rendertbl", 2, ImGuiTableFlags_None)) {
            ImGui::TableSetupColumn("##rl", ImGuiTableColumnFlags_WidthFixed, 95.f);
            ImGui::TableSetupColumn("##rc", ImGuiTableColumnFlags_WidthStretch);

            int masterGain = mod->getRenderParam(SoundModule::RENDER_MASTERGAIN_MILLIBEL);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kDim, "Master Gain");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.f);
            char gainLbl[24]; snprintf(gainLbl, sizeof(gainLbl), "%.1f dB", masterGain / 100.f);
            if (ImGui::SliderInt("##mastergain", &masterGain, -2400, 2400, gainLbl))
                m_ctrl->setMasterGain(masterGain);

            int stereoSep = mod->getRenderParam(SoundModule::RENDER_STEREOSEPARATION_PERCENT);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kDim, "Stereo Sep");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::SliderInt("##stereosep", &stereoSep, 0, 200, "%d%%"))
                m_ctrl->setStereoSeparation(stereoSep);

            static const char* kInterpNames[] = {"Default", "None (ZOH)", "Linear", "Cubic", "Sinc 8x"};
            static const int   kInterpVals[]  = {0, 1, 2, 4, 8};
            int interp    = mod->getRenderParam(SoundModule::RENDER_INTERPOLATIONFILTER_LENGTH);
            int interpIdx = 0;
            for (int i = 0; i < 5; ++i) if (kInterpVals[i] == interp) { interpIdx = i; break; }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kDim, "Interpolation");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::Combo("##interp", &interpIdx, kInterpNames, 5))
                m_ctrl->setInterpolation(kInterpVals[interpIdx]);

            int ramping = mod->getRenderParam(SoundModule::RENDER_VOLUMERAMPING_STRENGTH);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kDim, "Vol Ramping");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.f);
            char rampLbl[16]; snprintf(rampLbl, sizeof(rampLbl), ramping < 0 ? "Default" : "%d", ramping);
            if (ImGui::SliderInt("##ramping", &ramping, -1, 10, rampLbl))
                m_ctrl->setVolumeRamping(ramping);

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}
