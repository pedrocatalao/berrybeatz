#include "InstrumentView.h"
#include "Knob.h"
#include "imgui.h"
#include <libopenmpt/libopenmpt_ext.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// ─── Note name helper ────────────────────────────────────────────────────────

static const char* kNoteNames[12] = {
    "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
};

// note is 1-based (NOTE_MIN=1)
static void noteToStr(int note, char* buf, int bufsz) {
    if (note < 1 || note > 128) { snprintf(buf, bufsz, "---"); return; }
    int n = note - 1;           // 0-based
    int octave = n / 12;
    int semi   = n % 12;
    snprintf(buf, bufsz, "%s%d", kNoteNames[semi], octave);
}

// ─── Module / font setup ─────────────────────────────────────────────────────

void InstrumentView::setModule(openmpt::module_ext* mod, int instrCount) {
    m_mod        = mod;
    m_instrCount = instrCount;
    m_sel        = 1;
    m_dirty      = false;
    m_dragNode   = -1;
    m_playChannel = -1;
    m_playNote    = -1;
    if (m_mod && m_instrCount > 0) loadInstrument();
}

void InstrumentView::clearModule() {
    if (m_mod && m_playChannel >= 0) {
        auto* ia = static_cast<openmpt::ext::interactive*>(
            m_mod->get_interface(openmpt::ext::interactive_id));
        if (ia) ia->stop_note(m_playChannel);
    }
    m_mod         = nullptr;
    m_instrCount  = 0;
    m_sel         = 1;
    m_dirty       = false;
    m_dragNode    = -1;
    m_playChannel = -1;
    m_playNote    = -1;
    m_data        = InstrumentData{};
}

void InstrumentView::setFonts(ImFont* mono, ImFont* ui) {
    m_fontMono = mono;
    m_fontUI   = ui;
}

// ─── Load / save ─────────────────────────────────────────────────────────────

void InstrumentView::loadInstrument() {
    if (!m_mod) return;
    m_data  = bb_get_instrument_data(m_mod, m_sel);
    m_dirty = false;
}

void InstrumentView::saveInstrument() {
    if (!m_mod || !m_dirty) return;
    bb_set_instrument_data(m_mod, m_sel, m_data);
    m_dirty = false;
}

// ─── Top-level render ────────────────────────────────────────────────────────

void InstrumentView::render(float x, float y, float w, float h) {
    if (!m_mod || m_instrCount == 0) return;

    const float kListW = 200.f;
    drawList(x, y, kListW, h);
    drawEditor(x + kListW + 2.f, y, w - kListW - 2.f, h);

    // Flush any pending edits at end-of-frame
    if (m_dirty) saveInstrument();
}

// ─── Left: instrument list ───────────────────────────────────────────────────

void InstrumentView::drawList(float x, float y, float w, float h) {
    ImGui::SetCursorScreenPos({x, y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 20, 32, 255));
    if (ImGui::BeginChild("##instrList", {w, h}, false, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::SetScrollY(ImGui::GetScrollY()); // keep alive

        const ImVec4 kDim    = {0.5f, 0.5f, 0.65f, 1.f};
        const ImVec4 kAccent = {0.f, 0.78f, 1.f, 1.f};

        // Header
        ImGui::PushStyleColor(ImGuiCol_Text, kDim);
        ImGui::Text(" INSTRUMENTS");
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float rowH = 18.f;
        float cx   = ImGui::GetCursorScreenPos().x;
        float cy   = ImGui::GetCursorScreenPos().y;

        for (int i = 1; i <= m_instrCount; ++i) {
            bool sel = (i == m_sel);

            // Row background
            ImVec2 rMin = {cx, cy + (i - 1) * rowH};
            ImVec2 rMax = {cx + w - 4.f, rMin.y + rowH};
            if (sel)
                dl->AddRectFilled(rMin, rMax, IM_COL32(0, 80, 120, 200));
            else if (ImGui::IsMouseHoveringRect(rMin, rMax))
                dl->AddRectFilled(rMin, rMax, IM_COL32(40, 40, 60, 180));

            // Number badge
            char nbuf[8];
            snprintf(nbuf, sizeof(nbuf), "%3d", i);
            dl->AddText({rMin.x + 4.f, rMin.y + 1.f},
                        sel ? IM_COL32(0, 200, 255, 255) : IM_COL32(100, 100, 140, 255),
                        nbuf);

            // Name
            const char* name = (i == m_sel && m_data.valid) ? m_data.name : "";
            dl->AddText({rMin.x + 32.f, rMin.y + 1.f},
                        sel ? IM_COL32(220, 220, 240, 255) : IM_COL32(160, 160, 180, 255),
                        name[0] ? name : "(unnamed)");

            // Click to select
            ImGui::SetCursorScreenPos(rMin);
            ImGui::InvisibleButton(("##irow" + std::to_string(i)).c_str(), {w - 4.f, rowH});
            if (ImGui::IsItemClicked()) {
                if (i != m_sel) {
                    if (m_dirty) saveInstrument();
                    m_sel = i;
                    loadInstrument();
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─── Right: instrument editor ────────────────────────────────────────────────

void InstrumentView::drawEditor(float x, float y, float w, float h) {
    ImGui::SetCursorScreenPos({x, y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 14, 26, 255));
    if (!ImGui::BeginChild("##instrEd", {w, h}, false)) {
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    if (!m_data.valid) {
        ImGui::TextDisabled("No instrument data.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    const ImVec4 kDim = {0.5f, 0.5f, 0.65f, 1.f};
    const float  kPad = 6.f;
    float        cw   = w - kPad * 2.f;
    ImGui::SetCursorPosX(kPad);

    // ── A. Header ─────────────────────────────────────────────────────────────
    {
        // Number badge
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "[%02d]", m_sel);
        ImGui::TextColored({0.f, 0.78f, 1.f, 1.f}, "%s", nbuf);
        ImGui::SameLine(0, 8);

        // Name InputText
        ImGui::SetNextItemWidth(cw * 0.45f);
        if (ImGui::InputText("##iname", m_data.name, sizeof(m_data.name)))
            m_dirty = true;
        ImGui::SameLine(0, 8);

        // Filename (read-only)
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##ifname", m_data.filename, sizeof(m_data.filename),
                         ImGuiInputTextFlags_ReadOnly);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── B. NNA / DCT / DNA ───────────────────────────────────────────────────
    {
        const char* nnaitems[] = {"Note Cut","Continue","Note Off","Note Fade"};
        const char* dctitems[] = {"None","Note","Sample","Instrument","Plugin"};
        const char* dnaitems[] = {"Note Cut","Note Off","Note Fade"};

        ImGui::TextColored(kDim, "NNA:"); ImGui::SameLine(0, 4);
        int nnav = (int)m_data.nna;
        ImGui::SetNextItemWidth(90.f);
        if (ImGui::Combo("##nna", &nnav, nnaitems, 4))
            { m_data.nna = (uint8_t)nnav; m_dirty = true; }

        ImGui::SameLine(0, 12);
        ImGui::TextColored(kDim, "DCT:"); ImGui::SameLine(0, 4);
        int dctv = (int)m_data.dct;
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::Combo("##dct", &dctv, dctitems, 5))
            { m_data.dct = (uint8_t)dctv; m_dirty = true; }

        ImGui::SameLine(0, 12);
        ImGui::TextColored(kDim, "DNA:"); ImGui::SameLine(0, 4);
        int dnav = (int)m_data.dna;
        ImGui::SetNextItemWidth(90.f);
        if (ImGui::Combo("##dna", &dnav, dnaitems, 3))
            { m_data.dna = (uint8_t)dnav; m_dirty = true; }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── C. Volume + Panning (two columns) ────────────────────────────────────
    if (ImGui::BeginTable("##volpan", 2, ImGuiTableFlags_None, {cw, 0.f})) {
        ImGui::TableSetupColumn("##vc", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##pc", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        // ── Volume column ──────────────────────────────────────────────────
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(kDim, "VOLUME");
        ImGui::Spacing();

        // Global Vol knob
        {
            int gv = (int)m_data.globalVol;
            ImGui::BeginGroup();
            if (KnobWidget("##kGVol", &gv, 0, 64))
                { m_data.globalVol = (uint16_t)gv; m_dirty = true; }
            float lw = ImGui::CalcTextSize("G.Vol").x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (36.f - lw) * 0.5f);
            ImGui::TextColored(kDim, "G.Vol");
            ImGui::EndGroup();
            ImGui::SameLine(0, 12);
        }

        // Fadeout knob
        {
            int fo = (int)m_data.fadeOut;
            ImGui::BeginGroup();
            if (KnobWidget("##kFade", &fo, 0, 8192))
                { m_data.fadeOut = (uint16_t)fo; m_dirty = true; }
            float lw = ImGui::CalcTextSize("Fade").x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (36.f - lw) * 0.5f);
            ImGui::TextColored(kDim, "Fade");
            ImGui::EndGroup();
        }

        ImGui::Spacing();
        // Vol Swing slider
        {
            int vs = (int)m_data.volSwing;
            ImGui::TextColored(kDim, "Vol Swing"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::SliderInt("##vswing", &vs, 0, 100, "%d%%"))
                { m_data.volSwing = (uint8_t)vs; m_dirty = true; }
        }

        // ── Panning column ─────────────────────────────────────────────────
        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(kDim, "PANNING");
        ImGui::Spacing();

        // Panning enabled checkbox + knob
        {
            bool panEn = m_data.panEnabled;
            if (ImGui::Checkbox("##panen", &panEn))
                { m_data.panEnabled = panEn; m_dirty = true; }
            ImGui::SameLine(0, 4);
            ImGui::TextColored(kDim, "Pan");
            ImGui::SameLine(0, 8);

            int pv = (int)m_data.pan;
            bool disabled = !m_data.panEnabled;
            if (disabled) ImGui::BeginDisabled();
            if (KnobWidget("##kPan", &pv, 0, 256))
                { m_data.pan = (uint16_t)pv; m_dirty = true; }
            if (disabled) ImGui::EndDisabled();
        }

        ImGui::Spacing();
        // Pan Swing
        {
            int ps = (int)m_data.panSwing;
            ImGui::TextColored(kDim, "Pan Swing"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::SliderInt("##pswing", &ps, 0, 64))
                { m_data.panSwing = (uint8_t)ps; m_dirty = true; }
        }
        // PPS
        {
            int pps = (int)m_data.pps;
            ImGui::TextColored(kDim, "PPS");      ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::SliderInt("##pps", &pps, -32, 32))
                { m_data.pps = (int8_t)pps; m_dirty = true; }
        }
        // PPC (display only)
        {
            char ppcbuf[8];
            noteToStr((int)m_data.ppc + 1, ppcbuf, sizeof(ppcbuf));
            ImGui::TextColored(kDim, "PPC:"); ImGui::SameLine(0, 4);
            ImGui::TextUnformatted(ppcbuf);
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── D. Filter ─────────────────────────────────────────────────────────────
    {
        ImGui::TextColored(kDim, "FILTER");
        ImGui::Spacing();

        // Cutoff
        {
            bool ce = m_data.cutoffEnabled;
            if (ImGui::Checkbox("##cuten", &ce))
                { m_data.cutoffEnabled = ce; m_dirty = true; }
            ImGui::SameLine(0, 4);
            ImGui::TextColored(kDim, "Cutoff:"); ImGui::SameLine(0, 4);
            int cv = (int)m_data.cutoff;
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::SliderInt("##cut", &cv, 0, 127))
                { m_data.cutoff = (uint8_t)cv; m_dirty = true; }
        }
        // Resonance
        {
            bool re = m_data.resonanceEnabled;
            if (ImGui::Checkbox("##resen", &re))
                { m_data.resonanceEnabled = re; m_dirty = true; }
            ImGui::SameLine(0, 4);
            ImGui::TextColored(kDim, "Resonance:"); ImGui::SameLine(0, 4);
            int rv = (int)m_data.resonance;
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::SliderInt("##res", &rv, 0, 127))
                { m_data.resonance = (uint8_t)rv; m_dirty = true; }
        }
        // Cut/Res swing
        {
            int cs = (int)m_data.cutSwing;
            ImGui::TextColored(kDim, "Cut Swing"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::SliderInt("##csw", &cs, 0, 64))
                { m_data.cutSwing = (uint8_t)cs; m_dirty = true; }
            ImGui::SameLine(0, 12);
            int rs = (int)m_data.resSwing;
            ImGui::TextColored(kDim, "Res Swing"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::SliderInt("##rsw", &rs, 0, 64))
                { m_data.resSwing = (uint8_t)rs; m_dirty = true; }
        }
        // Filter mode
        {
            // Unchanged=0xFF, LowPass=0, HighPass=1
            // Map to combo index: 0=Unchanged, 1=LowPass, 2=HighPass
            const char* fmitems[] = {"Unchanged","Low Pass","High Pass"};
            int fmCombo = (m_data.filterMode == 0xFF) ? 0
                        : (m_data.filterMode == 0)    ? 1
                                                       : 2;
            ImGui::TextColored(kDim, "Filter Mode:"); ImGui::SameLine(0, 4);
            ImGui::SetNextItemWidth(110.f);
            if (ImGui::Combo("##fmode", &fmCombo, fmitems, 3)) {
                m_data.filterMode = (fmCombo == 0) ? 0xFF
                                  : (fmCombo == 1) ? 0
                                                   : 1;
                m_dirty = true;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── E. Resampling ─────────────────────────────────────────────────────────
    {
        // SRCMODE: 0=Nearest,1=Linear,2=Cubic,4=Sinc8,3=Sinc8LP,5=Default
        // Display order: Default(5), Nearest(0), Linear(1), Cubic(2), Sinc8(4), Sinc8+AA(3)
        const char* rsitems[] = {"Default","None (Nearest)","Linear","Cubic","Sinc8","Sinc8+AA"};
        const uint8_t rsvals[]  = {5, 0, 1, 2, 4, 3};
        int rsCombo = 0;
        for (int k = 0; k < 6; ++k)
            if (rsvals[k] == m_data.resampling) { rsCombo = k; break; }

        ImGui::TextColored(kDim, "Resampling:"); ImGui::SameLine(0, 4);
        ImGui::SetNextItemWidth(140.f);
        if (ImGui::Combo("##rsmode", &rsCombo, rsitems, 6)) {
            m_data.resampling = rsvals[rsCombo];
            m_dirty = true;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── F. Envelope editor (3 tabs) ──────────────────────────────────────────
    {
        const char* tabNames[] = {"Vol##et", "Pan##et", "Pitch##et"};
        ImGui::TextColored(kDim, "ENVELOPES");
        ImGui::Spacing();

        if (ImGui::BeginTabBar("##envtabs")) {
            for (int t = 0; t < 3; ++t) {
                if (ImGui::BeginTabItem(tabNames[t])) {
                    m_activeEnv = t;
                    InstrumentEnvData& env = (t == 0) ? m_data.volEnv
                                          : (t == 1) ? m_data.panEnv
                                                     : m_data.pitchEnv;

                    // Flags row
                    {
                        bool en = env.enabled;
                        if (ImGui::Checkbox("Enabled##en", &en))
                            { env.enabled = en; m_dirty = true; }
                        ImGui::SameLine(0, 10);

                        bool lp = env.loop;
                        if (ImGui::Checkbox("Loop##lp", &lp))
                            { env.loop = lp; m_dirty = true; }
                        ImGui::SameLine(0, 10);

                        bool su = env.sustain;
                        if (ImGui::Checkbox("Sustain##su", &su))
                            { env.sustain = su; m_dirty = true; }
                        ImGui::SameLine(0, 10);

                        bool ca = env.carry;
                        if (ImGui::Checkbox("Carry##ca", &ca))
                            { env.carry = ca; m_dirty = true; }

                        if (t == 2) { // pitch tab only
                            ImGui::SameLine(0, 10);
                            bool fi = env.filter;
                            if (ImGui::Checkbox("Filter##fi", &fi))
                                { env.filter = fi; m_dirty = true; }
                        }
                    }

                    // Loop / sustain node selectors
                    if (env.loop && !env.nodes.empty()) {
                        int nodeMax = (int)env.nodes.size() - 1;
                        int ls = (int)env.loopStart;
                        int le = (int)env.loopEnd;
                        ImGui::TextColored(kDim, "Loop:"); ImGui::SameLine(0,4);
                        ImGui::SetNextItemWidth(60.f);
                        if (ImGui::SliderInt("##ls", &ls, 0, nodeMax))
                            { env.loopStart = (uint8_t)ls; m_dirty = true; }
                        ImGui::SameLine(0,4);
                        ImGui::TextColored(kDim, "→"); ImGui::SameLine(0,4);
                        ImGui::SetNextItemWidth(60.f);
                        if (ImGui::SliderInt("##le", &le, 0, nodeMax))
                            { env.loopEnd = (uint8_t)le; m_dirty = true; }
                    }
                    if (env.sustain && !env.nodes.empty()) {
                        int nodeMax = (int)env.nodes.size() - 1;
                        int ss = (int)env.sustainStart;
                        int se = (int)env.sustainEnd;
                        ImGui::TextColored(kDim, "Sust:"); ImGui::SameLine(0,4);
                        ImGui::SetNextItemWidth(60.f);
                        if (ImGui::SliderInt("##ss", &ss, 0, nodeMax))
                            { env.sustainStart = (uint8_t)ss; m_dirty = true; }
                        ImGui::SameLine(0,4);
                        ImGui::TextColored(kDim, "→"); ImGui::SameLine(0,4);
                        ImGui::SetNextItemWidth(60.f);
                        if (ImGui::SliderInt("##se", &se, 0, nodeMax))
                            { env.sustainEnd = (uint8_t)se; m_dirty = true; }
                    }

                    // Graph
                    drawEnvelopeGraph(env, tabNames[t], cw, 160.f);

                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── G. Note/Sample keyboard ───────────────────────────────────────────────
    drawNoteKeyboard(cw, 120.f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─── Envelope graph ──────────────────────────────────────────────────────────

void InstrumentView::drawEnvelopeGraph(InstrumentEnvData& env, const char* typeLabel, float w, float h) {
    const ImU32 colBg       = IM_COL32(10, 10, 22, 255);
    const ImU32 colGrid     = IM_COL32(40, 40, 70, 255);
    const ImU32 colLine     = IM_COL32(0, 200, 255, 255);
    const ImU32 colNodeNorm = IM_COL32(0, 200, 255, 255);
    const ImU32 colNodeHov  = IM_COL32(255, 220, 0, 255);
    const ImU32 colMid      = IM_COL32(80, 80, 120, 180);
    const ImU32 colLoop     = IM_COL32(0, 80, 200, 60);
    const ImU32 colSustain  = IM_COL32(0, 160, 80, 60);
    const float nodeRadius  = 5.f;

    // Work out the max tick in the envelope (at least 64)
    int maxTick = 64;
    for (auto& n : env.nodes) maxTick = std::max(maxTick, (int)n.tick);
    maxTick = ((maxTick / 16) + 1) * 16; // round up to next multiple of 16

    // Map helper: tick/value → screen coords
    ImVec2 origin = ImGui::GetCursorScreenPos();
    auto toScreen = [&](int tick, int value) -> ImVec2 {
        float px = origin.x + (float)tick / (float)maxTick * w;
        float py = origin.y + (1.f - (float)value / 64.f) * h;
        return {px, py};
    };

    // InvisibleButton for interaction
    char btnId[32];
    snprintf(btnId, sizeof(btnId), "##envgraph_%s", typeLabel);
    ImGui::InvisibleButton(btnId, {w, h});
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(origin, {origin.x + w, origin.y + h}, colBg);

    // Grid: horizontal every 8 value units, vertical every 16 ticks
    for (int v = 0; v <= 64; v += 8) {
        ImVec2 a = toScreen(0,        v);
        ImVec2 b = toScreen(maxTick, v);
        dl->AddLine(a, b, colGrid, 0.5f);
    }
    for (int t = 0; t <= maxTick; t += 16) {
        ImVec2 a = toScreen(t, 0);
        ImVec2 b = toScreen(t, 64);
        dl->AddLine(a, b, colGrid, 0.5f);
    }

    // Middle line at value=32
    {
        ImVec2 a = toScreen(0,        32);
        ImVec2 b = toScreen(maxTick, 32);
        dl->AddLine(a, b, colMid, 1.f);
    }

    // Loop region
    if (env.loop && env.loopStart < (int)env.nodes.size() && env.loopEnd < (int)env.nodes.size()) {
        int t0 = env.nodes[env.loopStart].tick;
        int t1 = env.nodes[env.loopEnd].tick;
        ImVec2 p0 = toScreen(t0, 64);
        ImVec2 p1 = toScreen(t1, 0);
        dl->AddRectFilled(p0, p1, colLoop);
        dl->AddLine(toScreen(t0, 0), toScreen(t0, 64), IM_COL32(0, 100, 255, 200), 1.f);
        dl->AddLine(toScreen(t1, 0), toScreen(t1, 64), IM_COL32(0, 100, 255, 200), 1.f);
    }

    // Sustain region
    if (env.sustain && env.sustainStart < (int)env.nodes.size() && env.sustainEnd < (int)env.nodes.size()) {
        int t0 = env.nodes[env.sustainStart].tick;
        int t1 = env.nodes[env.sustainEnd].tick;
        ImVec2 p0 = toScreen(t0, 64);
        ImVec2 p1 = toScreen(t1, 0);
        dl->AddRectFilled(p0, p1, colSustain);
        dl->AddLine(toScreen(t0, 0), toScreen(t0, 64), IM_COL32(0, 200, 100, 200), 1.f);
        dl->AddLine(toScreen(t1, 0), toScreen(t1, 64), IM_COL32(0, 200, 100, 200), 1.f);
    }

    // Envelope lines
    for (int i = 0; i + 1 < (int)env.nodes.size(); ++i) {
        ImVec2 a = toScreen(env.nodes[i].tick,     env.nodes[i].value);
        ImVec2 b = toScreen(env.nodes[i+1].tick,   env.nodes[i+1].value);
        dl->AddLine(a, b, colLine, 1.5f);
    }

    // Nodes
    ImVec2 mp = ImGui::GetIO().MousePos;
    int hovNode = -1;
    for (int i = 0; i < (int)env.nodes.size(); ++i) {
        ImVec2 p = toScreen(env.nodes[i].tick, env.nodes[i].value);
        float dx = mp.x - p.x, dy = mp.y - p.y;
        if (hovered && std::sqrt(dx*dx + dy*dy) < 7.f) hovNode = i;
    }
    for (int i = 0; i < (int)env.nodes.size(); ++i) {
        ImVec2 p = toScreen(env.nodes[i].tick, env.nodes[i].value);
        bool isHov = (i == hovNode) || (i == m_dragNode);
        dl->AddCircleFilled(p, nodeRadius, isHov ? colNodeHov : colNodeNorm);
        dl->AddCircle(p, nodeRadius, IM_COL32(0, 0, 0, 180), 0, 1.f);
    }

    // Border
    dl->AddRect(origin, {origin.x + w, origin.y + h}, IM_COL32(60, 60, 100, 255));

    // ── Mouse interaction ────────────────────────────────────────────────────
    if (ImGui::IsItemActivated()) {
        m_dragNode = -1;
        // Find node under cursor
        for (int i = 0; i < (int)env.nodes.size(); ++i) {
            ImVec2 p = toScreen(env.nodes[i].tick, env.nodes[i].value);
            float dx = mp.x - p.x, dy = mp.y - p.y;
            if (std::sqrt(dx*dx + dy*dy) < 7.f) { m_dragNode = i; break; }
        }
        if (m_dragNode < 0) {
            // Insert new node at click position
            int clickTick  = (int)std::clamp((mp.x - origin.x) / w * maxTick, 0.f, (float)maxTick);
            int clickValue = (int)std::clamp((1.f - (mp.y - origin.y) / h) * 64.f, 0.f, 64.f);
            InstrumentEnvData::Node newNode{(uint16_t)clickTick, (uint8_t)clickValue};
            // Insert sorted by tick
            auto it = std::lower_bound(env.nodes.begin(), env.nodes.end(), newNode,
                [](const InstrumentEnvData::Node& a, const InstrumentEnvData::Node& b){
                    return a.tick < b.tick;
                });
            int idx = (int)(it - env.nodes.begin());
            env.nodes.insert(it, newNode);
            m_dragNode = idx;
            m_dirty    = true;
        }
    }

    if (active && m_dragNode >= 0 && m_dragNode < (int)env.nodes.size()) {
        int newTick  = (int)std::clamp((mp.x - origin.x) / w * maxTick, 0.f, (float)maxTick);
        int newValue = (int)std::clamp((1.f - (mp.y - origin.y) / h) * 64.f, 0.f, 64.f);

        // Clamp tick between neighbours
        int tickMin = (m_dragNode > 0)
            ? (int)env.nodes[m_dragNode - 1].tick + 1 : 0;
        int tickMax = (m_dragNode < (int)env.nodes.size() - 1)
            ? (int)env.nodes[m_dragNode + 1].tick - 1 : maxTick;
        newTick = std::clamp(newTick, tickMin, tickMax);

        env.nodes[m_dragNode].tick  = (uint16_t)newTick;
        env.nodes[m_dragNode].value = (uint8_t)newValue;
        m_dirty = true;
    }

    if (!active) m_dragNode = -1;

    // Right-click: delete node (keep at least 2)
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (hovNode >= 0 && (int)env.nodes.size() > 2) {
            env.nodes.erase(env.nodes.begin() + hovNode);
            m_dirty = true;
        }
    }
}

// ─── Note keyboard ───────────────────────────────────────────────────────────

void InstrumentView::drawNoteKeyboard(float w, float h) {
    // Show notes 25..84 (5 octaves: C-2 to B-6, 0-based indices 24..83)
    // 5 octaves × 7 white keys = 35 white keys
    constexpr int kNoteFirst = 25;  // 1-based
    constexpr int kNoteLast  = 84;  // 1-based
    constexpr int kOctaves   = 5;
    constexpr int kWhiteCount = kOctaves * 7;

    float whiteW = w / (float)kWhiteCount;
    float whiteH = h;
    float blackW = whiteW * 0.65f;
    float blackH = h * 0.60f;

    // Determine which semitones within octave are black keys
    // Semitone 0=C,1=C#,2=D,3=D#,4=E,5=F,6=F#,7=G,8=G#,9=A,10=A#,11=B
    // Black: 1,3,6,8,10
    static const bool kIsBlack[12] = {false,true,false,true,false,false,true,false,true,false,true,false};
    // White key index within octave (0-based among white keys)
    static const int  kWhiteIdx[12] = {0,0,1,1,2,3,3,4,4,5,5,6};
    // Black key x offset within white-key slot (fraction of whiteW, from left edge of the white key to its LEFT)
    // C# is between C(0) and D(1): offset from right edge of C = 0.66*whiteW
    static const float kBlackOffset[12] = {0,0.66f,0,0.66f,0,0,0.66f,0,0.66f,0,0.66f,0};

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Reserve space
    ImGui::InvisibleButton("##keyboard", {w, h});
    bool mouseDown     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    ImVec2 mp = ImGui::GetIO().MousePos;

    // get interactive interface
    auto* ia = m_mod ? static_cast<openmpt::ext::interactive*>(
        m_mod->get_interface(openmpt::ext::interactive_id)) : nullptr;

    // For detecting which key is pressed this frame
    int hitNote = -1;

    // ── First pass: white keys ────────────────────────────────────────────────
    int whiteIndex = 0;
    for (int note = kNoteFirst; note <= kNoteLast; ++note) {
        int semi = (note - 1) % 12;
        if (kIsBlack[semi]) continue;

        float kx = origin.x + whiteIndex * whiteW;
        float ky = origin.y;
        ImVec2 kMin = {kx,         ky};
        ImVec2 kMax = {kx + whiteW - 1.f, ky + whiteH};

        bool hov = (mp.x >= kMin.x && mp.x < kMax.x && mp.y >= kMin.y && mp.y < kMax.y);
        bool pressed = hov && mouseDown;

        ImU32 col = pressed ? IM_COL32(180, 220, 255, 255)
                  : hov     ? IM_COL32(210, 230, 255, 255)
                            : IM_COL32(230, 230, 240, 255);

        // Tint if has a sample assigned
        uint16_t smp = m_data.keyboard[note - 1];
        if (smp != 0 && !pressed && !hov)
            col = IM_COL32(180, 220, 200, 255);

        dl->AddRectFilled(kMin, kMax, col);
        dl->AddRect(kMin, kMax, IM_COL32(80, 80, 100, 255), 0.f, 0, 0.5f);

        // Middle C dot (note 61 = C-5)
        if (note == 61) {
            float dotX = kx + whiteW * 0.5f;
            float dotY = ky + whiteH - 10.f;
            dl->AddCircleFilled({dotX, dotY}, 3.f, IM_COL32(0, 160, 255, 200));
        }

        // Sample number text if assigned
        if (smp != 0 && m_fontMono) {
            char sbuf[8];
            snprintf(sbuf, sizeof(sbuf), "%d", (int)smp);
            ImVec2 tsz = m_fontMono->CalcTextSizeA(8.f, FLT_MAX, 0.f, sbuf);
            if (tsz.x < whiteW - 2.f)
                dl->AddText(m_fontMono, 8.f,
                    {kx + (whiteW - tsz.x) * 0.5f, ky + whiteH - tsz.y - 2.f},
                    IM_COL32(0, 60, 0, 200), sbuf);
        }

        if (hov && mouseDown) hitNote = note;

        ++whiteIndex;
    }

    // ── Second pass: black keys (drawn on top) ────────────────────────────────
    whiteIndex = 0;
    for (int note = kNoteFirst; note <= kNoteLast; ++note) {
        int semi = (note - 1) % 12;
        if (!kIsBlack[semi]) { ++whiteIndex; continue; }

        // The white key to the left
        int prevWhiteIdx = whiteIndex - 1;  // actually the current accumulated white index
        // Black key x: right portion of previous white key
        float kx = origin.x + (prevWhiteIdx + kBlackOffset[semi]) * whiteW - blackW * 0.5f + 7.f;
        float ky = origin.y;
        ImVec2 kMin = {kx,            ky};
        ImVec2 kMax = {kx + blackW,   ky + blackH};

        bool hov = (mp.x >= kMin.x && mp.x < kMax.x && mp.y >= kMin.y && mp.y < kMax.y);
        bool pressed = hov && mouseDown;

        ImU32 col = pressed ? IM_COL32(80, 120, 180, 255)
                  : hov     ? IM_COL32(60, 80, 120, 255)
                            : IM_COL32(30, 30, 40, 255);

        uint16_t smp = m_data.keyboard[note - 1];
        if (smp != 0 && !pressed && !hov)
            col = IM_COL32(20, 60, 40, 255);

        dl->AddRectFilled(kMin, kMax, col);
        dl->AddRect(kMin, kMax, IM_COL32(0, 0, 0, 255), 0.f, 0, 0.5f);

        if (smp != 0 && m_fontMono) {
            char sbuf[8];
            snprintf(sbuf, sizeof(sbuf), "%d", (int)smp);
            ImVec2 tsz = m_fontMono->CalcTextSizeA(8.f, FLT_MAX, 0.f, sbuf);
            if (tsz.x < blackW - 1.f)
                dl->AddText(m_fontMono, 8.f,
                    {kx + (blackW - tsz.x) * 0.5f, ky + blackH - tsz.y - 1.f},
                    IM_COL32(180, 220, 180, 200), sbuf);
        }

        if (hov && mouseDown) hitNote = note;
    }

    // ── Note play / stop logic ────────────────────────────────────────────────
    if (ia) {
        if (hitNote > 0 && hitNote != m_playNote) {
            // Stop previous note if any
            if (m_playChannel >= 0) ia->stop_note(m_playChannel);
            // Start new note: instrument is 1-based, note is 1-based
            m_playChannel = ia->play_note(m_sel, hitNote, 0.85, 0.5);
            m_playNote    = hitNote;
        } else if (hitNote < 0 && mouseReleased && m_playChannel >= 0) {
            ia->stop_note(m_playChannel);
            m_playChannel = -1;
            m_playNote    = -1;
        }
    }
}
