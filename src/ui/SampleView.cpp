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
    clearBrowserSample();
    m_ctrl        = ctrl;
    m_sampleCount = sampleCount;
    m_sel         = 1;
    m_dirty       = false;
    m_playChannel = -1;
    m_playNote    = -1;
    m_waveLen     = 0;
    m_waveMin.clear();
    m_waveMax.clear();
    std::fill(m_kbChannels, m_kbChannels + 128, -1);
    if (m_ctrl && m_sampleCount > 0) loadSample();
}

void SampleView::clearModule() {
    if (m_ctrl) {
        if (m_playChannel >= 0) m_ctrl->stopNote(m_playChannel);
        for (int i = 0; i < 128; ++i)
            if (m_kbChannels[i] >= 0) m_ctrl->stopNote(m_kbChannels[i]);
    }
    m_ctrl        = nullptr;
    m_sampleCount = 0;
    m_sel         = 1;
    m_dirty       = false;
    m_playChannel = -1;
    m_playNote    = -1;
    m_waveLen     = 0;
    m_waveMin.clear();
    m_waveMax.clear();
    m_data        = SampleData{};
}

void SampleView::midiNoteOn(int midiNote, int vel)
{
    if (!m_ctrl || midiNote < 0 || midiNote > 127) return;
    if (m_kbChannels[midiNote] >= 0) {
        m_ctrl->stopNote(m_kbChannels[midiNote]);
        m_kbChannels[midiNote] = -1;
    }
    double v = vel / 127.0;
    int opnote = midiNote + 1;  // note numbers are 1-based (NOTE_MIN=1)
    m_kbChannels[midiNote] = m_ctrl->playSample(m_sel, opnote, v, 0.5);
    if (onNeedAudio) onNeedAudio();
}

void SampleView::midiNoteOff(int midiNote)
{
    if (!m_ctrl || midiNote < 0 || midiNote > 127) return;
    if (m_kbChannels[midiNote] < 0) return;
    m_ctrl->stopNote(m_kbChannels[midiNote]);
    m_kbChannels[midiNote] = -1;
}

void SampleView::midiPitchBend(int raw14)
{
    m_pitchBend = raw14;
    if (!m_ctrl) return;
    double finetune = (raw14 - 8192) / 8192.0;
    for (int i = 0; i < 128; ++i)
        if (m_kbChannels[i] >= 0) m_ctrl->setNotePitch(m_kbChannels[i], finetune);
    if (m_playChannel >= 0) m_ctrl->setNotePitch(m_playChannel, finetune);
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

// ─── Browser preview ─────────────────────────────────────────────────────────

void SampleView::clearBrowserSample()
{
    m_browserMode     = false;
    m_browserName.clear();
    m_browserFrames   = 0;
    m_browserChannels = 1;
    m_browserSampleRate = 0;
}

void SampleView::loadBrowserSample(const int16_t* pcm, uint32_t frames, int channels,
                                    uint32_t sampleRate, const std::string& name)
{
    m_browserMode       = true;
    m_browserName       = name;
    m_browserFrames     = frames;
    m_browserChannels   = channels;
    m_browserSampleRate = sampleRate;

    // Build min/max waveform from raw PCM (same bucketing as bb_build_sample_waveform)
    constexpr int kBuckets = 1024;
    m_waveMin.assign(kBuckets, 0.f);
    m_waveMax.assign(kBuckets, 0.f);
    m_waveLen = kBuckets;

    if (frames > 0 && pcm) {
        for (int b = 0; b < kBuckets; ++b) {
            uint32_t s0 = (uint64_t)b       * frames / kBuckets;
            uint32_t s1 = (uint64_t)(b + 1) * frames / kBuckets;
            if (s1 <= s0) s1 = s0 + 1;
            float mn = 0.f, mx = 0.f;
            for (uint32_t s = s0; s < s1 && s < frames; ++s) {
                // Mix channels to mono for display
                float v = 0.f;
                for (int c = 0; c < channels; ++c)
                    v += pcm[s * channels + c] / 32768.f;
                v /= (float)channels;
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            m_waveMin[b] = mn;
            m_waveMax[b] = mx;
        }
    }
}

void SampleView::drawBrowserEditor(float x, float y, float w, float h)
{
    ImGui::SetCursorScreenPos({x, y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 14, 26, 255));
    if (!ImGui::BeginChild("##sampleEd", {w, h}, false)) {
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    const float kPad = 6.f;
    float cw = w - kPad * 2.f;
    ImGui::SetCursorPosX(kPad);

    // Header: name + metadata
    ImGui::TextColored({0.f, 0.78f, 1.f, 1.f}, "%s", m_browserName.c_str());
    ImGui::SameLine();
    char info[64];
    snprintf(info, sizeof(info), "  %s  %u Hz",
             m_browserChannels == 2 ? "Stereo" : "Mono", m_browserSampleRate);
    ImGui::TextDisabled("%s", info);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Waveform
    constexpr float kWaveH = 140.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 18, 255));
    if (ImGui::BeginChild("##bwave", {cw, kWaveH}, false)) {
        drawWaveform(cw, kWaveH);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // "Add to Song" button
    if (onAddToSong) {
        ImGui::SetCursorPosX(kPad);
        if (ImGui::Button("Add to Song", {120.f, 0.f}))
            onAddToSong();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─── Top-level render ────────────────────────────────────────────────────────

void SampleView::setSelection(int idx) {
    clearBrowserSample();
    if (!m_ctrl || m_sampleCount == 0) return;
    idx = std::max(1, std::min(idx, m_sampleCount));
    if (idx == m_sel) return;
    if (m_dirty) saveSample();
    m_sel = idx;
    loadSample();
}

void SampleView::render(float x, float y, float w, float h) {
    if (m_browserMode) {
        drawBrowserEditor(x, y, w, h);
        return;
    }
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

    // ── Keyboard ──────────────────────────────────────────────────────────────
    drawNoteKeyboard(cw, kKbdH);

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

// ─── Note keyboard ───────────────────────────────────────────────────────────

void SampleView::drawNoteKeyboard(float w, float h) {
    constexpr int kNoteFirst  = 25;   // 1-based
    constexpr int kNoteLast   = 84;   // 1-based
    constexpr int kOctaves    = 5;
    constexpr int kWhiteCount = kOctaves * 7;

    float whiteW = w / (float)kWhiteCount;
    float whiteH = h;
    float blackW = whiteW * 0.65f;
    float blackH = h * 0.60f;

    static const bool  kIsBlack[12]    = {false,true,false,true,false,false,true,false,true,false,true,false};
    static const float kBlackOffset[12]= {0,0.66f,0,0.66f,0,0,0.66f,0,0.66f,0,0.66f,0};

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mp = ImGui::GetIO().MousePos;

    ImGui::InvisibleButton("##smpKeyboard", {w, h});
    bool kbActive      = ImGui::IsItemActive();
    bool kbDeactivated = ImGui::IsItemDeactivated();

    bool hasCtrl = (m_ctrl != nullptr);

    // Determine which key is under cursor (black keys take priority)
    int hitNote = -1;
    if (kbActive) {
        // Black keys first
        int wi = 0;
        for (int note = kNoteFirst; note <= kNoteLast && hitNote < 0; ++note) {
            int semi = (note - 1) % 12;
            if (!kIsBlack[semi]) { ++wi; continue; }
            float kx = origin.x + ((wi - 1) + kBlackOffset[semi]) * whiteW - blackW * 0.5f + 7.f;
            float ky = origin.y;
            if (mp.x >= kx && mp.x < kx + blackW && mp.y >= ky && mp.y < ky + blackH)
                hitNote = note;
        }
        // White keys fallback
        if (hitNote < 0) {
            int wi2 = 0;
            for (int note = kNoteFirst; note <= kNoteLast && hitNote < 0; ++note) {
                int semi = (note - 1) % 12;
                if (kIsBlack[semi]) continue;
                float kx = origin.x + wi2 * whiteW;
                if (mp.x >= kx && mp.x < kx + whiteW - 1.f &&
                    mp.y >= origin.y && mp.y < origin.y + whiteH)
                    hitNote = note;
                ++wi2;
            }
        }
    }

    // ── First pass: white keys ────────────────────────────────────────────────
    int whiteIndex = 0;
    for (int note = kNoteFirst; note <= kNoteLast; ++note) {
        int semi = (note - 1) % 12;
        if (kIsBlack[semi]) continue;

        float kx = origin.x + whiteIndex * whiteW;
        float ky = origin.y;
        ImVec2 kMin = {kx,               ky};
        ImVec2 kMax = {kx + whiteW - 1.f, ky + whiteH};

        bool isHit  = (note == hitNote);
        bool isPlay = (note == m_playNote && m_playChannel >= 0);
        bool isKb   = (note >= 1 && note <= 128 && m_kbChannels[note-1] >= 0);
        bool hov    = (mp.x >= kMin.x && mp.x < kMax.x && mp.y >= kMin.y && mp.y < kMax.y);

        ImU32 col = (isHit || isPlay || isKb) ? IM_COL32(180, 220, 255, 255)
                  : hov                        ? IM_COL32(210, 230, 255, 255)
                                               : IM_COL32(230, 230, 240, 255);

        dl->AddRectFilled(kMin, kMax, col);
        dl->AddRect(kMin, kMax, IM_COL32(80, 80, 100, 255), 0.f, 0, 0.5f);

        if (note == 61) { // middle C dot
            dl->AddCircleFilled({kx + whiteW * 0.5f, ky + whiteH - 10.f},
                                3.f, IM_COL32(0, 160, 255, 200));
        }

        // Computer keyboard shortcut label
        {
            static const char* kLo[] = {"Z","","X","","C","V","","B","","N","","M"};
            static const char* kHi[] = {"Q","","W","","E","R","","T","","Y","","U"};
            int rel = (note - 1) - m_baseOctave * 12;
            const char* lbl = nullptr;
            if (rel >= 0  && rel < 12 && kLo[rel][0]) lbl = kLo[rel];
            if (rel >= 12 && rel < 24 && kHi[rel-12][0]) lbl = kHi[rel-12];
            if (lbl && m_fontMono) {
                ImVec2 tsz = m_fontMono->CalcTextSizeA(8.f, FLT_MAX, 0.f, lbl);
                dl->AddText(m_fontMono, 8.f,
                    {kx + (whiteW - tsz.x) * 0.5f, ky + 3.f},
                    IM_COL32(80, 80, 110, 200), lbl);
            }
        }

        // Note name label at the bottom of each C key
        if (semi == 0 && m_fontMono) {
            char nbuf[8];
            smpNoteToStr(note, nbuf, sizeof(nbuf));
            ImVec2 tsz = m_fontMono->CalcTextSizeA(8.f, FLT_MAX, 0.f, nbuf);
            if (tsz.x < whiteW - 2.f)
                dl->AddText(m_fontMono, 8.f,
                    {kx + (whiteW - tsz.x) * 0.5f, ky + whiteH - tsz.y - 2.f},
                    IM_COL32(0, 60, 0, 200), nbuf);
        }
        ++whiteIndex;
    }

    // ── Second pass: black keys (drawn on top) ────────────────────────────────
    whiteIndex = 0;
    for (int note = kNoteFirst; note <= kNoteLast; ++note) {
        int semi = (note - 1) % 12;
        if (!kIsBlack[semi]) { ++whiteIndex; continue; }

        float kx = origin.x + ((whiteIndex - 1) + kBlackOffset[semi]) * whiteW - blackW * 0.5f + 7.f;
        float ky = origin.y;
        ImVec2 kMin = {kx,          ky};
        ImVec2 kMax = {kx + blackW, ky + blackH};

        bool isHit  = (note == hitNote);
        bool isPlay = (note == m_playNote && m_playChannel >= 0);
        bool isKb   = (note >= 1 && note <= 128 && m_kbChannels[note-1] >= 0);
        bool hov    = (mp.x >= kMin.x && mp.x < kMax.x && mp.y >= kMin.y && mp.y < kMax.y);

        ImU32 col = (isHit || isPlay || isKb) ? IM_COL32(80, 120, 180, 255)
                  : hov                        ? IM_COL32(60, 80, 120, 255)
                                               : IM_COL32(30, 30, 40, 255);

        dl->AddRectFilled(kMin, kMax, col);
        dl->AddRect(kMin, kMax, IM_COL32(0, 0, 0, 255), 0.f, 0, 0.5f);

        // Key label for black keys
        {
            static const char* kLo[] = {"","S","","D","","","G","","H","","J",""};
            static const char* kHi[] = {"","2","","3","","","5","","6","","7",""};
            int rel = (note - 1) - m_baseOctave * 12;
            const char* lbl = nullptr;
            if (rel >= 0  && rel < 12 && kLo[rel][0]) lbl = kLo[rel];
            if (rel >= 12 && rel < 24 && kHi[rel-12][0]) lbl = kHi[rel-12];
            if (lbl && m_fontMono) {
                ImVec2 tsz = m_fontMono->CalcTextSizeA(7.f, FLT_MAX, 0.f, lbl);
                dl->AddText(m_fontMono, 7.f,
                    {kx + (blackW - tsz.x) * 0.5f, ky + 2.f},
                    IM_COL32(180, 180, 210, 200), lbl);
            }
        }
    }

    // ── Mouse play / stop logic ───────────────────────────────────────────────
    if (hasCtrl) {
        if (hitNote > 0 && hitNote != m_playNote) {
            if (m_playChannel >= 0) m_ctrl->stopNote(m_playChannel);
            if (onNeedAudio) onNeedAudio();
            m_playChannel = m_ctrl->playSample(m_sel, hitNote, 0.85, 0.5);
            m_playNote    = hitNote;
        }
        if (kbDeactivated && m_playChannel >= 0) {
            m_ctrl->stopNote(m_playChannel);
            m_playChannel = -1;
            m_playNote    = -1;
        }
    }

    // ── Computer keyboard play / stop logic ───────────────────────────────────
    if (hasCtrl && !ImGui::GetIO().WantTextInput) {
        struct NoteKey { ImGuiKey key; int semi; };
        static constexpr NoteKey kKeys[] = {
            {ImGuiKey_Z,0},{ImGuiKey_S,1},{ImGuiKey_X,2},{ImGuiKey_D,3},
            {ImGuiKey_C,4},{ImGuiKey_V,5},{ImGuiKey_G,6},{ImGuiKey_B,7},
            {ImGuiKey_H,8},{ImGuiKey_N,9},{ImGuiKey_J,10},{ImGuiKey_M,11},
            {ImGuiKey_Q,12},{ImGuiKey_2,13},{ImGuiKey_W,14},{ImGuiKey_3,15},
            {ImGuiKey_E,16},{ImGuiKey_R,17},{ImGuiKey_5,18},{ImGuiKey_T,19},
            {ImGuiKey_6,20},{ImGuiKey_Y,21},{ImGuiKey_7,22},{ImGuiKey_U,23},
        };
        for (auto& k : kKeys) {
            int note = m_baseOctave * 12 + k.semi + 1;
            if (note < 1 || note > 128) continue;
            if (ImGui::IsKeyPressed(k.key, false) && m_kbChannels[note-1] < 0) {
                if (onNeedAudio) onNeedAudio();
                m_kbChannels[note-1] = m_ctrl->playSample(m_sel, note, 0.85, 0.5);
            }
            if (ImGui::IsKeyReleased(k.key) && m_kbChannels[note-1] >= 0) {
                m_ctrl->stopNote(m_kbChannels[note-1]);
                m_kbChannels[note-1] = -1;
            }
        }
    }
}
