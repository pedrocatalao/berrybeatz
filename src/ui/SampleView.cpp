#include "SampleView.h"
#include "Knob.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>

// ─── Note name helper ────────────────────────────────────────────────────────

static const char* kSmpNoteNames[12] = {
    "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
};

static void smpNoteToStr(int note, char* buf, int bufsz) {
    if (note < 1 || note > 128) { snprintf(buf, bufsz, "---"); return; }
    int n      = note - 1;
    int octave = n / 12;
    int semi   = n % 12;
    snprintf(buf, bufsz, "%s%d", kSmpNoteNames[semi], octave);
}

// ─── Module / font setup ─────────────────────────────────────────────────────

void SampleView::setController(ModuleController* ctrl, int sampleCount) {
    m_ctrl        = ctrl;
    m_sampleCount = sampleCount;
    m_sel         = 1;
    m_dirty       = false;
    m_waveLen  = 0;
    m_waveMin.clear();
    m_waveMax.clear();
    if (m_ctrl && m_sampleCount > 0) loadSample();
}

void SampleView::clearModule() {
    // Audio engine is stopped before clearModule is called; just reset held state.
    m_ctrl        = nullptr;
    m_sampleCount = 0;
    m_sel         = 1;
    m_dirty       = false;
    m_waveLen     = 0;
    m_waveMin.clear();
    m_waveMax.clear();
    m_data        = SampleData{};
}

void SampleView::midiNoteOn(int midiNote, int vel)
{
    if (midiNote < 0 || midiNote > 127) return;
    float v = vel / 127.f;
    int opnote = midiNote + 1;
    if (onEnqueuePlay) onEnqueuePlay(m_sel, opnote, v, 0.5f, midiNote);
    if (onNeedAudio) onNeedAudio();
}

void SampleView::midiNoteOff(int midiNote)
{
    if (midiNote < 0 || midiNote > 127) return;
    if (onEnqueueStop) onEnqueueStop(midiNote);
}

void SampleView::midiPitchBend(int raw14)
{
    m_pitchBend = raw14;
    float finetune = (raw14 - 8192) / 8192.f;
    if (onEnqueuePitch) onEnqueuePitch(finetune);
}

void SampleView::setFonts(ImFont* mono, ImFont* ui) {
    m_fontMono = mono;
    m_fontUI   = ui;
}

// ─── Load / save ─────────────────────────────────────────────────────────────

void SampleView::loadSample() {
    if (!m_ctrl || !m_ctrl->module()) return;
    m_data  = bb_get_sample_data(m_ctrl->module(), m_sel);
    bb_build_sample_waveform(m_ctrl->module(), m_sel, m_waveMin, m_waveMax, m_waveLen);
    m_dirty = false;
}

void SampleView::saveSample() {
    if (!m_ctrl || !m_ctrl->module() || !m_dirty) return;
    bb_set_sample_data(m_ctrl->module(), m_sel, m_data);
    m_dirty = false;
}

// ─── Top-level render ────────────────────────────────────────────────────────

void SampleView::setSelection(int idx) {
    if (!m_ctrl || m_sampleCount == 0) return;
    idx = std::max(1, std::min(idx, m_sampleCount));
    if (idx == m_sel) return;
    if (m_dirty) saveSample();
    m_sel = idx;
    loadSample();
}

void SampleView::render(float x, float y, float w, float h) {
    if (!m_ctrl || m_sampleCount == 0) return;

    drawEditor(x, y, w, h);
    if (m_dirty) saveSample();
}

// ─── Sample editor ───────────────────────────────────────────────────────────

void SampleView::drawEditor(float x, float y, float w, float h) {
    ImGui::SetCursorScreenPos({x, y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 14, 26, 255));
    if (!ImGui::BeginChild("##sampleEd", {w, h}, false)) {
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    if (!m_data.valid) {
        ImGui::TextDisabled("No sample data.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    const ImVec4 kDim = {0.5f, 0.5f, 0.65f, 1.f};
    const float  kPad = 6.f;
    float        cw   = w - kPad * 2.f;
    ImGui::SetCursorPosX(kPad);

    // ── Row 1: Header ─────────────────────────────────────────────────────────
    {
        // Number badge
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "[%02d]", m_sel);
        ImGui::TextColored({0.f, 0.78f, 1.f, 1.f}, "%s", nbuf);
        ImGui::SameLine(0, 8);

        // Filename (read-only, dimmed, right-aligned — estimate its width first)
        float fnWidth = 140.f;
        if (m_fontMono)
            fnWidth = m_fontMono->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.f,
                                                m_data.filename[0] ? m_data.filename : "--------").x + 8.f;

        // Name InputText fills remaining width
        float badgeX    = ImGui::GetCursorPosX();
        float nameWidth = cw - (badgeX - kPad) - fnWidth - 8.f;
        ImGui::SetNextItemWidth(std::max(60.f, nameWidth));
        if (ImGui::InputText("##sname", m_data.name, sizeof(m_data.name)))
            m_dirty = true;
        ImGui::SameLine(0, 8);

        // Filename read-only
        ImGui::SetNextItemWidth(fnWidth);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 120, 150, 255));
        ImGui::InputText("##sfname", m_data.filename, sizeof(m_data.filename),
                         ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Row 2: Info bar ───────────────────────────────────────────────────────
    {
        char infobuf[128];
        snprintf(infobuf, sizeof(infobuf),
                 "Length: %u  |  %s  |  %s  |  C-5: %u Hz",
                 m_data.length,
                 m_data.isStereo ? "Stereo" : "Mono",
                 m_data.is16bit  ? "16-bit" : "8-bit",
                 m_data.c5speed);
        ImGui::TextColored(kDim, "%s", infobuf);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Properties table (2 columns) ─────────────────────────────────────────
    if (ImGui::BeginTable("##smpProps", 2, ImGuiTableFlags_None, {cw, 0.f})) {
        ImGui::TableSetupColumn("##smpL", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##smpR", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        // ── LEFT column ───────────────────────────────────────────────────────
        ImGui::TableSetColumnIndex(0);

        // Global Volume knob
        {
            int gv = (int)m_data.globalVol;
            if (KnobEdit("##kSGVol", &gv, 0, 64, false, "G.Vol"))
                { m_data.globalVol = (uint16_t)gv; m_dirty = true; }
            ImGui::SameLine(0, 12);
        }

        // Volume knob
        {
            int vol = (int)m_data.volume;
            if (KnobEdit("##kSVol", &vol, 0, 256, false, "Volume"))
                { m_data.volume = (uint16_t)vol; m_dirty = true; }
            ImGui::SameLine(0, 12);
        }

        // Pan knob (bipolar, center=128)
        {
            int pv = (int)m_data.pan;
            if (KnobEdit("##kSPan", &pv, 0, 256, /*bipolar=*/true, "Pan")) {
                m_data.pan        = (uint16_t)pv;
                m_data.panEnabled = true;
                m_dirty           = true;
            }
            ImGui::SameLine(0, 12);
        }

        // Pitch bend display
        KnobDisplay("##pb", (m_pitchBend - 8192) / 8192.f, "P.Bend");

        ImGui::Spacing();

        // Fine Tune
        {
            int ft = (int)m_data.fineTune;
            ImGui::TextColored(kDim, "Fine Tune"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::SliderInt("##sft", &ft, -128, 127))
                { m_data.fineTune = (int8_t)ft; m_dirty = true; }
        }

        // C-5 Speed
        {
            int c5 = (int)m_data.c5speed;
            ImGui::TextColored(kDim, "C-5 Speed"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::DragInt("##sc5", &c5, 1.f, 1, 192000))
                { m_data.c5speed = (uint32_t)std::max(1, c5); m_dirty = true; }
        }

        // Relative Tone
        {
            int rt = (int)m_data.relativeTone;
            ImGui::TextColored(kDim, "Rel. Tone"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::DragInt("##srt", &rt, 1.f, -120, 120))
                { m_data.relativeTone = (int8_t)rt; m_dirty = true; }
        }

        // ── RIGHT column ──────────────────────────────────────────────────────
        ImGui::TableSetColumnIndex(1);

        // Loop type
        {
            int lt = (int)m_data.loopType;
            ImGui::TextColored(kDim, "Loop"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(110.f);
            if (ImGui::Combo("##sloop", &lt, "None\0Forward\0Ping-Pong\0"))
                { m_data.loopType = (uint8_t)lt; m_dirty = true; }
        }
        if (m_data.loopType > 0) {
            int ls = (int)m_data.loopStart;
            ImGui::TextColored(kDim, "L.Start"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::DragInt("##slstart", &ls, 1.f, 0, (int)m_data.length))
                { m_data.loopStart = (uint32_t)std::max(0, ls); m_dirty = true; }
            int le = (int)m_data.loopEnd;
            ImGui::TextColored(kDim, "L.End"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::DragInt("##slend", &le, 1.f, 0, (int)m_data.length))
                { m_data.loopEnd = (uint32_t)std::max(0, le); m_dirty = true; }
        }

        // Sustain loop type
        {
            int st = (int)m_data.sustainLoopType;
            ImGui::TextColored(kDim, "Sust."); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(110.f);
            if (ImGui::Combo("##ssust", &st, "None\0Forward\0Ping-Pong\0"))
                { m_data.sustainLoopType = (uint8_t)st; m_dirty = true; }
        }
        if (m_data.sustainLoopType > 0) {
            int ss = (int)m_data.sustainStart;
            ImGui::TextColored(kDim, "S.Start"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::DragInt("##ssstart", &ss, 1.f, 0, (int)m_data.length))
                { m_data.sustainStart = (uint32_t)std::max(0, ss); m_dirty = true; }
            int se = (int)m_data.sustainEnd;
            ImGui::TextColored(kDim, "S.End"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::DragInt("##ssend", &se, 1.f, 0, (int)m_data.length))
                { m_data.sustainEnd = (uint32_t)std::max(0, se); m_dirty = true; }
        }

        ImGui::Separator();

        // Vibrato type
        {
            int vt = (int)m_data.vibType;
            ImGui::TextColored(kDim, "Vibrato"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(110.f);
            if (ImGui::Combo("##svib", &vt, "None\0Sine\0Square\0Ramp Up\0Ramp Down\0Random\0"))
                { m_data.vibType = (uint8_t)vt; m_dirty = true; }
        }
        if (m_data.vibType > 0) {
            int vr = (int)m_data.vibRate;
            ImGui::TextColored(kDim, "Speed"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::SliderInt("##svrate", &vr, 0, 64))
                { m_data.vibRate = (uint8_t)vr; m_dirty = true; }

            int vd = (int)m_data.vibDepth;
            ImGui::TextColored(kDim, "Depth"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::SliderInt("##svdepth", &vd, 0, 32))
                { m_data.vibDepth = (uint8_t)vd; m_dirty = true; }

            int vs = (int)m_data.vibSweep;
            ImGui::TextColored(kDim, "Sweep"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::SliderInt("##svsweep", &vs, 0, 255))
                { m_data.vibSweep = (uint8_t)vs; m_dirty = true; }
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Waveform display ──────────────────────────────────────────────────────
    constexpr float kWaveH = 140.f;
    constexpr float kKbdH  = 120.f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 18, 255));
    if (ImGui::BeginChild("##wave", {cw, kWaveH}, false)) {
        drawWaveform(cw, kWaveH);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─── Waveform display ────────────────────────────────────────────────────────

void SampleView::drawWaveform(float w, float h) {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(origin, {origin.x + w, origin.y + h}, IM_COL32(10, 10, 18, 255));

    // Zero line
    float midY = origin.y + h * 0.5f;
    dl->AddLine({origin.x, midY}, {origin.x + w, midY}, IM_COL32(40, 40, 60, 255), 1.f);

    if (m_waveLen > 0) {
        // Waveform bars: one bar per waveform bucket
        for (int b = 0; b < m_waveLen; ++b) {
            float bx  = origin.x + (float)b / (float)m_waveLen * w;
            float bx2 = origin.x + (float)(b + 1) / (float)m_waveLen * w;
            if (bx2 <= bx) bx2 = bx + 1.f;

            // Map [-1,1] → [bottom, top]
            float yMax = origin.y + (1.f - (m_waveMax[b] * 0.5f + 0.5f)) * h;
            float yMin = origin.y + (1.f - (m_waveMin[b] * 0.5f + 0.5f)) * h;
            if (yMax > yMin) std::swap(yMax, yMin);

            dl->AddRectFilled({bx, yMax}, {bx2, yMin + 1.f}, IM_COL32(0, 190, 255, 200));
        }

        // Loop overlay
        if (m_data.loopType > 0 && m_data.loopEnd > m_data.loopStart && m_data.length > 0) {
            float lx0 = origin.x + (float)m_data.loopStart / (float)m_data.length * w;
            float lx1 = origin.x + (float)m_data.loopEnd   / (float)m_data.length * w;
            dl->AddRectFilled({lx0, origin.y}, {lx1, origin.y + h}, IM_COL32(0, 80, 180, 50));
            dl->AddLine({lx0, origin.y}, {lx0, origin.y + h}, IM_COL32(0, 120, 255, 200), 1.f);
            dl->AddLine({lx1, origin.y}, {lx1, origin.y + h}, IM_COL32(0, 120, 255, 200), 1.f);
        }

        // Sustain overlay
        if (m_data.sustainLoopType > 0 && m_data.sustainEnd > m_data.sustainStart && m_data.length > 0) {
            float sx0 = origin.x + (float)m_data.sustainStart / (float)m_data.length * w;
            float sx1 = origin.x + (float)m_data.sustainEnd   / (float)m_data.length * w;
            dl->AddRectFilled({sx0, origin.y}, {sx1, origin.y + h}, IM_COL32(0, 150, 80, 50));
            dl->AddLine({sx0, origin.y}, {sx0, origin.y + h}, IM_COL32(0, 200, 100, 200), 1.f);
            dl->AddLine({sx1, origin.y}, {sx1, origin.y + h}, IM_COL32(0, 200, 100, 200), 1.f);
        }
    } else {
        const char* msg = m_data.isOPL ? "OPL / FM Instrument" : "No sample data";
        ImVec2 tsz = ImGui::CalcTextSize(msg);
        dl->AddText({origin.x + (w - tsz.x) * 0.5f, origin.y + (h - tsz.y) * 0.5f},
                    IM_COL32(80, 80, 100, 255), msg);
    }

    // Border
    dl->AddRect(origin, {origin.x + w, origin.y + h}, IM_COL32(40, 40, 70, 255));

    // Advance cursor past the waveform area
    ImGui::Dummy({w, h});
}

