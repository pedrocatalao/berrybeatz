#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "Theme.h"
#include "RenderSettings.h"
#include "TrackerView.h"
#include "ModuleController.h"
#include "PatternEditor.h"
#include "SoundModule.h"
#include <imgui.h>
#include <algorithm>

// ── OpenMPT "Blue (FT2)" colour scheme — sourced from Theme.h ─────────────────
static constexpr ImVec4 kBgNormal   = kCellBgNormal;
static constexpr ImVec4 kTextNormal = kCellTextNormal;
static constexpr ImVec4 kTextCurRow = kCellTextCurRow;
static constexpr ImVec4 kBgHilight  = kCellBgBeat;
static constexpr ImVec4 kBgMeasure  = kCellBgMeasure;
static constexpr ImVec4 kColNote    = kCellNote;
static constexpr ImVec4 kColInstr   = kCellInstr;
static constexpr ImVec4 kColVolume  = kCellVolume;
static constexpr ImVec4 kColGlobal  = kCellEffect;
static constexpr ImVec4 kDim        = kCellDim;
static constexpr ImVec4 kChHeader   = kCellChanHdr;
static constexpr ImVec4 kOrderBg    = kCellOrderBg;
static constexpr ImVec4 kOrderCur   = kCellOrderCur;

static constexpr ImU32 kVuLoLit    = kVuCyanLit;
static constexpr ImU32 kVuLoUnlit  = kVuCyanUnlit;
static constexpr ImU32 kVuMedLit   = kVuPinkLit;
static constexpr ImU32 kVuMedUnlit = kVuPinkUnlit;
static constexpr ImU32 kVuHiLit    = kVuVioletLit;
static constexpr ImU32 kVuHiUnlit  = kVuVioletUnlit;

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr float PTBAR1_H   = 28.0f;
static constexpr float PTBAR2_H   = 28.0f;
static constexpr float PTBAR3_H   = 28.0f;
static constexpr float ORDER_H    = 26.0f;
static constexpr float CHANNAME_H = 18.0f;
static constexpr float VU_H       = 13.0f;
static constexpr float WAVE_H     = 28.0f;
static constexpr float PLUGIN_H   = 14.0f;

// ── TrackerView ───────────────────────────────────────────────────────────────

TrackerView::TrackerView(ModuleState& state) : m_state(state) {}


// ── Toolbar helpers ───────────────────────────────────────────────────────────

static void tip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
        ImGui::SetTooltip("%s", text);
}

static void togBtn(const char* lbl, bool& state, bool enabled = true) {
    if (!enabled) ImGui::BeginDisabled();
    bool wasActive = state;
    if (wasActive) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(64, 64, 160, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 190, 255));
    }
    if (ImGui::Button(lbl)) state = !state;
    if (wasActive) ImGui::PopStyleColor(2);
    if (!enabled) ImGui::EndDisabled();
}

static void tbSep() {
    ImGui::SameLine(0, 5);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  h = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddLine(
        {p.x, p.y + 2.f}, {p.x, p.y + h - 2.f},
        IM_COL32(46, 46, 90, 220), 1.0f);
    ImGui::Dummy({1.0f, h});
    ImGui::SameLine(0, 5);
}

static void pushToolbarStyle() {
    ImGui::PushStyleColor(ImGuiCol_Button,         IM_COL32( 45,  45,  68, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32( 65,  65,  98, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32( 80,  80, 120, 255));
    ImGui::PushStyleColor(ImGuiCol_Text,           IM_COL32(208, 208, 224, 255));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   IM_COL32( 80,  80,  90, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32( 80,  80, 112, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32( 13,  13,  30, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32( 26,  26,  46, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32( 46,  46,  90, 255));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      IM_COL32(208, 208, 224, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    {8.0f, 2.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,     {3.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
}

static void popToolbarStyle() {
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(10);
}

// ── Main render entry point ───────────────────────────────────────────────────

void TrackerView::render(float x, float y, float w, float h) {
    if (m_fontMono) ImGui::PushFont(m_fontMono);
    m_charW   = ImGui::CalcTextSize("A").x;
    m_rowNumW = m_charW * 3.0f + 10.0f;
    static constexpr float kCellChars[4] = {11.f, 8.f, 5.f, 3.f};
    static constexpr float kCellPad[4]  = {26.f, 20.f, 14.f, 8.f};
    m_cellW = m_charW * kCellChars[m_detailView] + kCellPad[m_detailView];
    m_rowH    = ImGui::GetTextLineHeightWithSpacing();
    if (m_fontMono) ImGui::PopFont();

    m_chanH  = CHANNAME_H
             + (m_showVU        ? VU_H     : 0.f)
             + (m_showWave      ? WAVE_H   : 0.f)
             + (m_showPluginRow ? PLUGIN_H : 0.f);
    m_patTop = y + PTBAR1_H + PTBAR2_H + PTBAR3_H + ORDER_H + m_chanH;

    ImGuiWindowFlags noDecoFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    const ImVec4 kBarBg{0.f, 0.f, 0.f, 0.f};

    // Pattern toolbar row 1
    ImGui::SetNextWindowPos ({x, y});
    ImGui::SetNextWindowSize({w, PTBAR1_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBarBg);
    ImGui::Begin("##ptbar1", nullptr, noDecoFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawPatternToolbar1(x, y, w);
    ImGui::End();

    // Pattern toolbar row 2 (edit ops)
    ImGui::SetNextWindowPos ({x, y + PTBAR1_H});
    ImGui::SetNextWindowSize({w, PTBAR3_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBarBg);
    ImGui::Begin("##ptbar2", nullptr, noDecoFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawPatternToolbar3(x, y + PTBAR1_H, w);
    ImGui::End();

    // Pattern toolbar row 3 (instrument/step)
    ImGui::SetNextWindowPos ({x, y + PTBAR1_H + PTBAR3_H});
    ImGui::SetNextWindowSize({w, PTBAR2_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBarBg);
    ImGui::Begin("##ptbar3", nullptr, noDecoFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawPatternToolbar2(x, y + PTBAR1_H + PTBAR3_H, w);
    ImGui::End();

    // Box behind order list + pattern panel
    {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        float        bY = y + PTBAR1_H + PTBAR2_H + PTBAR3_H;
        ImVec2       p0 = {x + 7.f, bY};
        ImVec2       p1 = {x + w - 2.f,   y + h};
        bg->AddRectFilled(p0, p1, IM_COL32(14, 14, 26, 255));
    }

    // Order list
    ImGui::SetNextWindowPos ({x + 26.f + 7.f, y + PTBAR1_H + PTBAR2_H + PTBAR3_H});
    ImGui::SetNextWindowSize({w - 26.f - 7.f, ORDER_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kOrderBg);
    ImGui::Begin("##orders", nullptr, noDecoFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleColor();
    drawOrderList(x, y + PTBAR1_H + PTBAR2_H + PTBAR3_H, w);
    ImGui::End();

    // Pattern panel — channel headers are the frozen first row of the table
    float chanY = y + PTBAR1_H + PTBAR2_H + PTBAR3_H + ORDER_H;
    float patH  = h - (chanY - y);
    ImGui::SetNextWindowPos ({x + 7.f, chanY});
    ImGui::SetNextWindowSize({w - 7.f, patH});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          IM_COL32(  0,   0,   0,   0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        IM_COL32( 40,  85, 145, 200));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32( 55, 112, 175, 230));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  IM_COL32( 66, 130, 205, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0)); // pushed last → popped first below
    ImGui::Begin("##pattern", nullptr, noDecoFlags);
    ImGui::PopStyleVar(1);   // WindowPadding — no longer needed after Begin
    ImGui::PopStyleColor(1); // WindowBg — no longer needed after Begin

    if (!ImGui::GetIO().WantTextInput) {
        auto prevCur = m_cur;
        processInput();
        m_editCursorMoved = (m_cur.row != prevCur.row || m_cur.channel != prevCur.channel);
    } else {
        m_editCursorMoved = false;
    }

    drawPatternPanel(x, chanY, w, patH);
    ImGui::End();
    ImGui::PopStyleColor(4); // scrollbar colors — must outlive End()

    // Plugin settings popup (anchored to chanstrip window for ID stability)
    ImGui::SetNextWindowPos({x, chanY});
    ImGui::SetNextWindowSize({1, 1}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##plughost", nullptr, noDecoFlags | ImGuiWindowFlags_NoNav);
    if (m_pluginPopupCh >= 0) {
        m_pluginPopup.open();
        m_pluginPopupCh = -1;
    }
    if (m_pluginPopup.begin("Plugin Settings", 300.f, 76.f)) {
        ImGui::Spacing();
        ImGui::TextColored(kDim, "No plugin assigned.");
        m_pluginPopup.end();
    }
    ImGui::End();

}

// ── Pattern toolbar row 1 ─────────────────────────────────────────────────────

void TrackerView::drawPatternToolbar1(float /*x*/, float /*y*/, float /*w*/) {
    bool has = m_ctrl != nullptr;
    if (m_fontUI) ImGui::PushFont(m_fontUI);
    pushToolbarStyle();
    ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) * 0.5f);

    auto actBtn = [&](const char* lbl, bool enabled = true) -> bool {
        if (!enabled) ImGui::BeginDisabled();
        bool r = ImGui::Button(lbl);
        if (!enabled) ImGui::EndDisabled();
        return r;
    };

    if (actBtn("Insert##ins", has)) {
        if (onInsertPatternRequested) onInsertPatternRequested();
    }
    tip("Insert pattern");
    ImGui::SameLine();
    if (actBtn("Play Cursor##playcur", has)) {
        int32_t row = m_editMode ? m_cur.row : m_state.current_row.load();
        if (onPatternPlayFromCursor) onPatternPlayFromCursor(m_state.current_order.load(), row, m_loopPattern);
    }
    tip("Play from cursor (stops at pattern end, loops if Loop is on)");
    ImGui::SameLine();
    if (actBtn("Play Start##playstart", has)) {
        if (onPatternPlayFromStart) onPatternPlayFromStart(m_state.current_order.load(), m_loopPattern);
    }
    tip("Play pattern from start");
    ImGui::SameLine();
    if (actBtn("Stop##stop2", has)) {
        if (onStopRequested) onStopRequested();
    }
    tip("Stop playback");
    ImGui::SameLine();
    if (actBtn("Play Row##playrow", has)) {
        int32_t ord = m_state.current_order.load();
        int32_t row = m_cur.row;
        advanceCursor();
        if (onPatternPlayRow) onPatternPlayRow(ord, row);
    }
    tip("Play current row and advance cursor");
    ImGui::SameLine();

    bool wasRec = m_editMode;
    if (wasRec) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(160, 30, 30, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(190, 50, 50, 255));
    }
    if (actBtn("Record##rec", has)) setEditMode(!m_editMode);
    if (wasRec) ImGui::PopStyleColor(2);
    tip("Toggle record mode (Enter)");

    tbSep();
    togBtn("VU##vu", m_showVU);   tip("Show/hide channel VU meters");
    ImGui::SameLine();
    togBtn("WF##wf", m_showWave); tip("Show/hide channel waveform");
    ImGui::SameLine();
    togBtn("FX##fx", m_showPluginRow); tip("Show/hide plugin row");

    popToolbarStyle();
    if (m_fontUI) ImGui::PopFont();
}

// ── Pattern toolbar row 3 ─────────────────────────────────────────────────────

void TrackerView::drawPatternToolbar3(float /*x*/, float /*y*/, float /*w*/) {
    bool has = m_ctrl != nullptr;
    if (m_fontUI) ImGui::PushFont(m_fontUI);
    pushToolbarStyle();
    ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) * 0.5f);

    auto actBtn = [&](const char* lbl, bool enabled = true) -> bool {
        if (!enabled) ImGui::BeginDisabled();
        bool r = ImGui::Button(lbl);
        if (!enabled) ImGui::EndDisabled();
        return r;
    };

    if (actBtn("Undo##undo", has && m_undo.canUndo())) { m_undo.undo(*m_ctrl); } tip("Undo");
    ImGui::SameLine();
    if (actBtn("Props##props",  has)) {} tip("Pattern properties");
    ImGui::SameLine();
    if (actBtn("Expand##exp",   has)) {} tip("Expand pattern");
    ImGui::SameLine();
    if (actBtn("Shrink##shr",   has)) {} tip("Shrink pattern");

    tbSep();
    { bool prev = m_loopPattern; togBtn("Loop##lp", m_loopPattern); if (m_loopPattern != prev && onLoopToggled) onLoopToggled(m_loopPattern); }    tip("Loop current pattern");
    ImGui::SameLine();
    if (m_editMode) ImGui::BeginDisabled();
    togBtn("Follow##fw", m_followSong);             tip("Follow playback cursor");
    if (m_editMode) ImGui::EndDisabled();
    tbSep();
    {
        bool wasActive = m_detailView > 0;
        if (wasActive) {
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(64, 64, 160, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 190, 255));
        }
        if (ImGui::Button("Detail##detail"))
            m_detailView = (m_detailView + 1) % 4;
        if (wasActive) ImGui::PopStyleColor(2);
    }
    static const char* detailTips[] = {
        "Note + Instrument + Volume + Effect",
        "Note + Instrument + Volume",
        "Note + Instrument",
        "Note only",
    };
    tip(detailTips[m_detailView]);
    tbSep();
    togBtn("Overflow##ovf", m_overflowPaste);  tip("Overflow paste");

    popToolbarStyle();
    if (m_fontUI) ImGui::PopFont();
}

// ── Pattern toolbar row 2 ─────────────────────────────────────────────────────

void TrackerView::drawPatternToolbar2(float /*x*/, float /*y*/, float /*w*/) {
    bool has = m_ctrl != nullptr;
    if (m_fontUI) ImGui::PushFont(m_fontUI);
    pushToolbarStyle();
    ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) * 0.5f);

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kDim, "Instr");
    ImGui::SameLine(0, 4);
    {
        std::vector<std::string> names;
        int total = 0;
        {
            std::lock_guard<std::mutex> lk(m_state.meta_mutex);
            if (m_state.num_instruments > 0) {
                names = m_state.instrument_names;
                total = m_state.num_instruments;
            } else {
                names = m_state.sample_names;
                total = m_state.num_samples;
            }
        }
        char preview[80];
        if (m_selectedInstrument <= 0)
            snprintf(preview, sizeof(preview), "No instrument");
        else {
            int idx = m_selectedInstrument - 1;
            if (idx < (int)names.size() && !names[idx].empty())
                snprintf(preview, sizeof(preview), "%02d: %s", m_selectedInstrument.load(), names[idx].c_str());
            else
                snprintf(preview, sizeof(preview), "%02d", m_selectedInstrument.load());
        }
        ImGui::SetNextItemWidth(160.0f);
        bool comboOpen = ImGui::BeginCombo("##instr", preview,
            ImGuiComboFlags_HeightLarge | ImGuiComboFlags_NoArrowButton);
        // Small custom down-arrow
        {
            ImVec2 rMin = ImGui::GetItemRectMin(), rMax = ImGui::GetItemRectMax();
            float cx = rMax.x - 10.f, cy = (rMin.y + rMax.y) * 0.5f;
            constexpr float s = 4.f;
            ImGui::GetWindowDrawList()->AddTriangleFilled(
                {cx - s, cy - s * 0.55f}, {cx + s, cy - s * 0.55f}, {cx, cy + s * 0.65f},
                IM_COL32(180, 180, 200, 200));
        }
        if (comboOpen) {
            if (ImGui::Selectable("No instrument", m_selectedInstrument <= 0))
                m_selectedInstrument = 0;
            if (m_selectedInstrument <= 0) ImGui::SetItemDefaultFocus();
            for (int i = 1; i <= total; i++) {
                char lbl[80];
                int idx = i - 1;
                if (idx < (int)names.size() && !names[idx].empty())
                    snprintf(lbl, sizeof(lbl), "%02d: %s", i, names[idx].c_str());
                else
                    snprintf(lbl, sizeof(lbl), "%02d", i);
                bool sel = (m_selectedInstrument == i);
                if (ImGui::Selectable(lbl, sel)) {
                    m_selectedInstrument = i;
                    if (onInstrumentChanged) onInstrumentChanged(i);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    tip("Current instrument — click to pick from list");

    ImGui::SameLine(0, 6);
    ImGui::BeginDisabled();
    ImGui::Button("Plugin##plugin2");
    ImGui::EndDisabled();
    tip("Plugin selector (not yet available)");

    tbSep();

    ImGui::TextColored(kDim, "Step");
    ImGui::SameLine(0, 6);
    ImGui::TextColored(ImVec4(0.82f, 0.82f, 0.88f, 1.f), "%d", m_editStep);
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(44.0f);
    {
        int sv = m_editStep;
        if (ImGui::InputInt("##step2", &sv, 1, 1))
            m_editStep = std::clamp(sv, 0, 16);
    }
    tip("Edit step (- / + keys)");

    tbSep();

    ImGui::TextColored(kDim, "Pattern");
    ImGui::SameLine(0, 4);
    int32_t curPat = m_ctrl ? m_state.current_pattern.load() : -1;
    std::string patName;
    if (curPat >= 0) {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        if (curPat < (int32_t)m_state.pattern_names.size())
            patName = m_state.pattern_names[curPat];
    }
    char patBuf[64] = {};
    strncpy(patBuf, patName.c_str(), 63);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::BeginDisabled(!has);
    ImGui::InputText("##patname", patBuf, 64);
    ImGui::EndDisabled();

    popToolbarStyle();
    if (m_fontUI) ImGui::PopFont();
}

// ── Order list ────────────────────────────────────────────────────────────────

void TrackerView::drawOrderList(float /*x*/, float /*y*/, float /*w*/) {
    int32_t norders  = m_state.num_orders.load();
    int32_t curOrder = m_state.current_order.load();

    if (g_fontOrderRow) ImGui::PushFont(g_fontOrderRow);

    if (norders <= 0) {
        ImGui::SetCursorPosY(4);
        ImGui::TextColored(kDim, "  Open a module file  (File > Open or drag & drop)");
        if (g_fontOrderRow) ImGui::PopFont();
        return;
    }

    float boxW = m_charW * 4.2f;
    float boxH = ORDER_H - 4.0f;

    ImGui::SetCursorPosY(2);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {2.0f, 0.0f});

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int32_t i = 0; i < norders; i++) {
        bool    isCur = (i == curOrder);
        int32_t pat   = m_ctrl ? m_ctrl->module()->getOrderPattern(i) : i;

        char id[12], txt[8];
        snprintf(id,  sizeof(id),  "##o%d", i);
        snprintf(txt, sizeof(txt), "%02d",  pat);

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(id, {boxW, boxH});
        bool hov     = ImGui::IsItemHovered();
        bool pressed = ImGui::IsItemActive();
        if (ImGui::IsItemClicked() && onOrderSeekRequested)
            onOrderSeekRequested(i);

        // Background
        ImVec4 bg = isCur ? kOrderCur : kOrderBg;
        if (pressed)   bg = {bg.x+.2f, bg.y+.2f, bg.z+.3f, 1.f};
        else if (hov)  bg = {bg.x+.1f, bg.y+.1f, bg.z+.2f, 1.f};
        if (isCur || hov || pressed)
            dl->AddRectFilled(p, {p.x + boxW, p.y + boxH},
                              ImGui::ColorConvertFloat4ToU32(bg));

        // Text — explicitly centred, no FramePadding interference
        ImVec2 ts  = ImGui::CalcTextSize(txt);
        float  tx  = p.x + (boxW - ts.x) * 0.5f + 1.f;
        float  ty  = p.y + (boxH - ts.y) * 0.5f;
        dl->AddText({tx, ty}, ImGui::ColorConvertFloat4ToU32(isCur ? kTextCurRow : kTextNormal), txt);

        ImGui::SameLine(0, 2);
    }

    ImGui::PopStyleVar();
    if (g_fontOrderRow) ImGui::PopFont();
}

// ── Stereo VU ─────────────────────────────────────────────────────────────────

void TrackerView::drawStereoVU(ImDrawList* dl, float x, float y, float w, float h,
                                float lvlL, float lvlR) {
    constexpr float barW   = 2.0f;
    constexpr float stride = 3.0f;
    float cx    = x + w * 0.5f;
    int   N     = std::max(1, int((w * 0.5f - 1.0f) / stride));
    float bH    = h - 1.0f;
    float normL = std::min(lvlL / 1.2f, 1.0f);
    float normR = std::min(lvlR / 1.2f, 1.0f);

    // Glow pass — additive blending, only lit segments
    if (g_glowVU > 0.f) {
        const float opa = g_glowVU * 0.5f;
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }, nullptr);
        for (int i = 0; i < N; i++) {
            float frac = float(i) / float(N);
            int gr, gg, gb;
            if      (frac >= 0.80f) { gr = 200; gg =   0; gb = 255; }
            else if (frac >= 0.60f) { gr = 255; gg =  30; gb = 180; }
            else                    { gr =   0; gg = 220; gb = 255; }
            float rx = cx + 1.0f + float(i) * stride;
            float lx = cx - 1.0f - float(i) * stride - barW;
            if (normR > frac) {
                dl->AddRectFilled({rx - 16, y - 6}, {rx + barW + 16, y + bH + 6}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({rx - 11, y - 5}, {rx + barW + 11, y + bH + 5}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({rx -  7, y - 3}, {rx + barW +  7, y + bH + 3}, IM_COL32(gr, gg, gb, (uint8_t)( 5 * opa)));
                dl->AddRectFilled({rx -  4, y - 2}, {rx + barW +  4, y + bH + 2}, IM_COL32(gr, gg, gb, (uint8_t)( 7 * opa)));
                dl->AddRectFilled({rx -  2, y - 1}, {rx + barW +  2, y + bH + 1}, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
                dl->AddRectFilled({rx -  1, y    }, {rx + barW +  1, y + bH    }, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
            }
            if (normL > frac) {
                dl->AddRectFilled({lx - 16, y - 6}, {lx + barW + 16, y + bH + 6}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({lx - 11, y - 5}, {lx + barW + 11, y + bH + 5}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({lx -  7, y - 3}, {lx + barW +  7, y + bH + 3}, IM_COL32(gr, gg, gb, (uint8_t)( 5 * opa)));
                dl->AddRectFilled({lx -  4, y - 2}, {lx + barW +  4, y + bH + 2}, IM_COL32(gr, gg, gb, (uint8_t)( 7 * opa)));
                dl->AddRectFilled({lx -  2, y - 1}, {lx + barW +  2, y + bH + 1}, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
                dl->AddRectFilled({lx -  1, y    }, {lx + barW +  1, y + bH    }, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
            }
        }
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }, nullptr);
    }

    // Normal bars
    for (int i = 0; i < N; i++) {
        float frac = float(i) / float(N);
        ImU32 litCol, unlitCol;
        if      (frac >= 0.80f) { litCol = kVuHiLit;  unlitCol = kVuHiUnlit;  }
        else if (frac >= 0.60f) { litCol = kVuMedLit; unlitCol = kVuMedUnlit; }
        else                    { litCol = kVuLoLit;  unlitCol = kVuLoUnlit;  }
        float rx = cx + 1.0f + float(i) * stride;
        dl->AddRectFilled({rx, y}, {rx + barW, y + bH}, normR > frac ? litCol : unlitCol);
        float lx = cx - 1.0f - float(i) * stride - barW;
        dl->AddRectFilled({lx, y}, {lx + barW, y + bH}, normL > frac ? litCol : unlitCol);
    }
}


// ── Pattern panel ─────────────────────────────────────────────────────────────

void TrackerView::drawPatternPanel(float /*x*/, float /*y*/, float /*w*/, float /*h*/) {
    if (!m_ctrl || m_state.num_channels.load() <= 0) {
        ImGui::TextColored(kDim, " No module loaded.");
        return;
    }
    auto* mod = m_ctrl->module();

    int32_t curPat  = m_state.current_pattern.load();
    int32_t curRow  = m_state.current_row.load();
    int32_t nch     = m_state.num_channels.load();
    int32_t nrows   = mod->getPatternNumRows(curPat);

    int rpb = mod->getPatternRowsPerBeat(curPat);
    if (rpb <= 0) rpb = 4;
    int rpm = mod->getPatternRowsPerMeasure(curPat);
    if (rpm <= 0) rpm = 16;

    int32_t curOrder = m_state.current_order.load();

    struct Slot { int32_t pat, nrows, startAbs, orderIdx; int rpb, rpm; };
    std::vector<Slot> slots;
    int32_t totalRows = 0;
    int32_t curAbsRow = curRow;

    if (m_continuousScroll) {
        int32_t numOrders = m_state.num_orders.load();
        slots.reserve(numOrders);
        for (int32_t o = 0; o < numOrders; ++o) {
            int32_t p  = (int32_t)mod->getOrderPattern(o);
            int32_t pr = (p >= 0) ? (int32_t)mod->getPatternNumRows(p) : 0;
            if (pr <= 0) continue;
            int prpb = mod->getPatternRowsPerBeat(p);    if (prpb <= 0) prpb = 4;
            int prpm = mod->getPatternRowsPerMeasure(p); if (prpm <= 0) prpm = 16;
            if (o == curOrder) curAbsRow = totalRows + curRow;
            slots.push_back({p, pr, totalRows, o, prpb, prpm});
            totalRows += pr;
        }
    }
    if (slots.empty()) {
        slots.push_back({curPat, nrows, 0, curOrder, rpb, rpm});
        totalRows = nrows;
        curAbsRow = curRow;
    }

    // Click to move edit cursor — works in both modes.
    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && m_rowH > 0.f && m_tableOriginY > 0.f) {
        ImVec2 mp = ImGui::GetIO().MousePos;
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        const float sb = ImGui::GetStyle().ScrollbarSize;
        bool inGrid = ImGui::IsMouseHoveringRect(
            {wp.x, wp.y + m_chanH}, {wp.x + ws.x - sb, wp.y + ws.y - sb}, false);
        if (inGrid) {
            float relY  = mp.y - m_tableOriginY + m_scrollY;
            float step  = m_colStep > 0.f ? m_colStep : (m_cellW + 2.f * ImGui::GetStyle().CellPadding.x);
            float relX  = mp.x - (m_tableOriginX - m_hscroll);
            int   absRow = int(relY / m_rowH);
            int   ch     = int(relX / step);
            if (ch >= 0 && ch < nch && absRow >= 0 && absRow < totalRows) {
                int32_t clickLocalR   = absRow;
                if (m_continuousScroll) {
                    for (auto& s : slots) {
                        if (absRow >= s.startAbs && absRow < s.startAbs + s.nrows) {
                            clickLocalR = absRow - s.startAbs;
                            if (s.orderIdx != curOrder && onOrderSeekRequested)
                                onOrderSeekRequested(s.orderIdx);
                            m_cur.row = clickLocalR; m_cur.channel = ch; m_cur.col = EditCol::Note;
                            setEditMode(true);
                            break;
                        }
                    }
                } else if (clickLocalR < nrows) {
                    m_cur.row = clickLocalR; m_cur.channel = ch; m_cur.col = EditCol::Note;
                    setEditMode(true);
                }
            }
        }
    }

    int totalCols = 1 + nch;
    ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX;

    float rowH  = m_rowH;  // pre-computed with mono font in render()
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float kSbSize = ImGui::GetStyle().ScrollbarSize;
    float visH  = avail.y - m_chanH - kSbSize;  // exclude frozen header and horizontal scrollbar

    // Fetch channel data for the frozen header row
    const VuSnapshot& snap = m_state.vuBuf[1 - m_state.vuWriteIdx.load(std::memory_order_acquire)];
    const auto& vu          = snap.vu;
    const auto& channelWave = snap.channelWave;
    std::vector<std::string> chNames(nch);
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        for (int ch = 0; ch < nch; ch++) {
            if (ch < (int)m_state.channel_names.size() && !m_state.channel_names[ch].empty())
                chNames[ch] = std::to_string(ch + 1) + ": " + m_state.channel_names[ch];
            else
                chNames[ch] = "Channel " + std::to_string(ch + 1);
        }
    }

    ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight,  IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.f, ImGui::GetStyle().CellPadding.y));
    bool tableOpen = ImGui::BeginTable("##pat", totalCols, tFlags, avail);
    if (!tableOpen) { ImGui::PopStyleVar(); ImGui::PopStyleColor(2); return; }

    ImGui::TableSetupScrollFreeze(1, 1);  // freeze col 0 + row 0 (channel headers)
    ImGui::TableSetupColumn("##row", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, m_rowNumW);
    for (int ch = 0; ch < nch; ch++) {
        char id[12]; snprintf(id, sizeof(id), "##c%d", ch);
        ImGui::TableSetupColumn(id, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, m_cellW);
    }

    m_hscroll = ImGui::GetScrollX();

    // ── Frozen channel header row ──────────────────────────────────────────────
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        uint64_t newMuteState = m_muteState;
        int32_t plugClickCh = -1;

        ImGui::TableNextRow(ImGuiTableRowFlags_None, m_chanH);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 0, 0), 0);

        ImGui::TableSetColumnIndex(0);
        ImGui::Dummy({m_rowNumW, m_chanH});

        for (int ch = 0; ch < nch; ch++) {
            ImGui::TableSetColumnIndex(ch + 1);
            ImVec2 p  = ImGui::GetCursorScreenPos();
            float  cw = m_cellW;
            bool muted = (m_muteState >> ch) & 1;

            // Background + divider
            ImU32 hdrCol = muted ? IM_COL32(20,20,30,255) : IM_COL32(30,30,60,255);
            float padX   = ImGui::GetStyle().CellPadding.x;
            float hdrTop = p.y;
            float hdrBot = hdrTop + m_chanH + 4.f;
            dl->AddRectFilled(p, {p.x + cw - 1, p.y + CHANNAME_H}, hdrCol);
            // Fill left and right padding gutters with the same dark blue as pattern rows
            dl->AddRectFilled({p.x - padX, hdrTop}, {p.x,          hdrBot}, IM_COL32(46, 46, 92, 200));
            dl->AddRectFilled({p.x + cw,   hdrTop}, {p.x + cw + padX, hdrBot}, IM_COL32(46, 46, 92, 200));
            dl->AddLine({p.x,          hdrTop}, {p.x,          hdrBot}, IM_COL32(46, 46, 92, 200));
            dl->AddLine({p.x + cw - 1, hdrTop}, {p.x + cw - 1, hdrBot}, IM_COL32(46, 46, 92, 200));

            // Channel name, centred
            float textH = ImGui::GetTextLineHeight();
            ImU32 textCol = muted ? IM_COL32(60,60,60,255) : IM_COL32(200,200,120,255);
            ImVec2 ts = ImGui::CalcTextSize(chNames[ch].c_str());
            dl->AddText({p.x + (cw - ts.x) * 0.5f, p.y + (CHANNAME_H - textH) * 0.5f},
                        textCol, chNames[ch].c_str());

            // Mute / solo click target (both detected on press to avoid release-timing conflicts)
            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(0x10000 + ch);
            ImGui::InvisibleButton("##mute", {cw, CHANNAME_H});
            {
                ImVec2 pMax = {p.x + cw, p.y + float(CHANNAME_H)};
                bool hov = ImGui::IsMouseHoveringRect(p, pMax);
                bool dbl = hov && ImGui::IsMouseDoubleClicked(0);
                bool sng = hov && ImGui::IsMouseClicked(0) && !dbl;
                uint64_t allOthersMask = ((uint64_t(1) << nch) - 1) & ~(uint64_t(1) << ch);
                if (dbl) {
                    bool wasPlaying = !((m_preClickMuteState >> ch) & 1);
                    bool wasSoloed  = wasPlaying && (m_preClickMuteState == allOthersMask);
                    if (wasSoloed || !wasPlaying)
                        newMuteState = 0;
                    else
                        newMuteState = allOthersMask;
                } else if (sng) {
                    m_preClickMuteState = m_muteState;
                    bool isSoloed = m_muteState == allOthersMask;
                    if (isSoloed)
                        newMuteState = 0;
                    else
                        newMuteState ^= (uint64_t(1) << ch);
                }
            }

            // VU
            float vuTop = p.y + CHANNAME_H;
            if (m_showVU)
                drawStereoVU(dl, p.x + 1, vuTop, cw - 2, VU_H, vu[ch].left, vu[ch].right);

            // Oscilloscope waveform
            float vuBot = vuTop + (m_showVU ? VU_H : 0.f);
            if (m_showWave && channelWave[ch].count > 1) {
                float wy    = vuBot;
                dl->AddRectFilled({p.x + 1, wy}, {p.x + cw - 1, wy + WAVE_H}, IM_COL32(20, 20, 33, 255));
                float cy    = wy + WAVE_H * 0.5f;
                float halfH = WAVE_H * 0.5f - 2.f;

                // Show only a portion of the buffer so individual waves are visible
                static constexpr int kDisplaySamples = 160;
                int total = channelWave[ch].count;
                int displayN = std::min(total, kDisplaySamples);

                // Oscilloscope trigger: find first upward zero crossing in the
                // lead-in window so the waveform appears stable (no flickering)
                int start = 0;
                int searchEnd = total - displayN;
                for (int i = 1; i < searchEnd; ++i) {
                    if (channelWave[ch].samples[i - 1] <= 0.f && channelWave[ch].samples[i] > 0.f) {
                        start = i;
                        break;
                    }
                }

                // Normalize by peak, partial floor so quiet signals stay readable
                float peak = 0.f;
                for (int i = start; i < start + displayN; ++i)
                    peak = std::max(peak, std::abs(channelWave[ch].samples[i]));
                float norm = (peak > 0.0001f) ? halfH / std::max(peak, 0.15f) : 0.f;
                // Fade when channel is silent
                float chAmp = (vu[ch].left + vu[ch].right) * 0.5f;
                norm *= std::min(chAmp * 50.0f, 1.0f);

                ImU32 wavCol = muted ? IM_COL32(80, 80, 80, 120) : IM_COL32(190, 190, 200, 220);
                float xscale = (cw - 2.f) / float(displayN);
                ImVec2 prev  = {p.x + 1, cy - channelWave[ch].samples[start] * norm};
                for (int i = 1; i < displayN; ++i) {
                    float fx = p.x + float(i) * xscale;
                    float fy = cy - channelWave[ch].samples[start + i] * norm;
                    dl->AddLine(prev, {fx, fy}, wavCol);
                    prev = {fx, fy};
                }
            }

            // Plugin row
            float plugTop = vuBot + (m_showWave ? WAVE_H : 0.f);
            if (m_showPluginRow) {
                dl->AddRectFilled({p.x + 1, plugTop}, {p.x + cw - 1, plugTop + PLUGIN_H},
                                  IM_COL32(20, 20, 35, 255));
                {
                    ImVec2 ts = ImGui::CalcTextSize("\xe2\x80\x93");
                    dl->AddText({p.x + (cw - ts.x) * 0.5f, plugTop + (PLUGIN_H - ts.y) * 0.5f},
                                IM_COL32(60, 60, 80, 255), "\xe2\x80\x93");
                }
                ImGui::SetCursorScreenPos({p.x, plugTop});
                if (ImGui::InvisibleButton("##plug", {cw, PLUGIN_H}))
                    plugClickCh = ch;
            }
            ImGui::PopID();

            // Claim the full header height so the row is tall enough
            ImGui::SetCursorScreenPos({p.x, p.y + m_chanH});
            ImGui::Dummy({0.f, 0.f});
        }

        if (newMuteState != m_muteState) {
            m_muteState = newMuteState;
            m_state.channel_mute_mask.store(newMuteState);
        }
        if (plugClickCh >= 0)
            m_pluginPopupCh = plugClickCh;
    }

    float currentScrollY = ImGui::GetScrollY();

    // Manual vertical scroll while hovered → suppress follow for 1 second.
    // Ignore when horizontal component dominates (trackpad horizontal swipe bleed).
    const float dt = ImGui::GetIO().DeltaTime;
    {
        const float wy = ImGui::GetIO().MouseWheel;
        const float wx = ImGui::GetIO().MouseWheelH;
        if (std::abs(wy) > std::abs(wx) &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        {
            m_scrollSuppressTimer = 1.0f;
        }
    }
    if (m_scrollSuppressTimer > 0.f)
        m_scrollSuppressTimer = std::max(0.f, m_scrollSuppressTimer - dt);

    bool  playing    = m_state.is_playing.load();
    bool  patChanged = (curPat != m_lastPattern);
    bool  rowRewound = (playing && curAbsRow < m_lastAbsRow - 2);
    bool  navSnap    = m_navSnapPending; m_navSnapPending = false;
    // In continuous mode pattern boundaries are seamless — only snap on actual
    // backward jumps (loop restart / seek), not on natural forward progression.
    bool  needsSnap  = m_continuousScroll ? (rowRewound || navSnap) : (patChanged || rowRewound || navSnap);

    bool  forceScroll  = false;
    float newScrollY   = currentScrollY;
    bool  forceScrollX = false;
    float newScrollX   = m_hscroll;

    if (m_editMode) {
        int32_t editAbs = 0;
        for (auto& s : slots)
            if (s.orderIdx == curOrder) { editAbs = s.startAbs + m_cur.row; break; }
        if (m_editCursorMoved) {
            int32_t rowDelta = editAbs - m_lastEditAbsRow;
            if (rowDelta != 0) {
                const float kZone     = 3.f * rowH;
                float       cursorTop = float(editAbs) * rowH;
                float       cursorBot = cursorTop + rowH;
                bool nearBottom = cursorBot  > currentScrollY + visH - kZone;
                bool nearTop    = cursorTop  < currentScrollY + kZone;
                if ((rowDelta > 0 && nearBottom) || (rowDelta < 0 && nearTop)) {
                    newScrollY  = currentScrollY + float(rowDelta) * rowH;
                    forceScroll = true;
                }
            }
            int32_t chDelta = m_cur.channel - m_lastEditChannel;
            if (chDelta != 0 && m_colStep > 0.f) {
                float visW      = avail.x - m_rowNumW - kSbSize;
                float kZoneX    = 2.f * m_colStep;
                float cursorL   = float(m_cur.channel) * m_colStep;
                float cursorR   = cursorL + m_colStep;
                bool nearRight  = cursorR > m_hscroll + visW - kZoneX;
                bool nearLeft   = cursorL < m_hscroll + kZoneX;
                if ((chDelta > 0 && nearRight) || (chDelta < 0 && nearLeft)) {
                    newScrollX   = m_hscroll + float(chDelta) * m_colStep;
                    forceScrollX = true;
                }
            }
        }
        m_lastEditAbsRow  = editAbs;
        m_lastEditChannel = m_cur.channel;
    } else if (needsSnap) {
        newScrollY  = float(curAbsRow) * rowH - visH * 0.5f + rowH * 2.5f;
        forceScroll = true;
    } else if (playing && m_followSong && m_scrollSuppressTimer == 0.f) {
        float targetY = float(curAbsRow) * rowH - visH * 0.5f + rowH * 2.5f;
        newScrollY  = currentScrollY + (targetY - currentScrollY) * std::min(1.0f, 8.0f * dt);
        forceScroll = true;
    }

    if (forceScroll) {
        newScrollY = std::max(0.f, std::min(newScrollY, float(totalRows) * rowH));
        ImGui::SetScrollY(newScrollY);
        m_scrollY = newScrollY;
    } else {
        m_scrollY = currentScrollY;
    }
    if (forceScrollX) {
        float maxScrollX = std::max(0.f, float(nch) * (m_colStep > 0.f ? m_colStep : m_cellW) - (avail.x - m_rowNumW - kSbSize));
        newScrollX = std::max(0.f, std::min(newScrollX, maxScrollX));
        ImGui::SetScrollX(newScrollX);
        m_hscroll = newScrollX;
    }

    m_lastPattern = curPat;
    m_lastRow     = curRow;
    m_lastAbsRow  = curAbsRow;

    if (m_fontMono) ImGui::PushFont(m_fontMono);

    // Animate per-slot dim factors (1.0 = active, 0.5 = fully dim, 900ms linear fade)
    {
        const float dt       = ImGui::GetIO().DeltaTime;
        const float fadeStep = 0.5f * std::min(dt / 0.3f, 1.0f);
        for (auto& s : slots) {
            if (s.orderIdx == curOrder) {
                m_slotDim[s.orderIdx] = 1.0f;
            } else {
                auto [it, inserted] = m_slotDim.emplace(s.orderIdx, 0.5f);
                if (!inserted)
                    it->second = std::max(it->second - fadeStep, 0.5f);
            }
        }
    }

    // For edit cursor: absolute row within the continuous scroll space
    int32_t curOrderSlotStart = 0;
    for (auto& s : slots)
        if (s.orderIdx == curOrder) { curOrderSlotStart = s.startAbs; break; }
    int32_t editAbsRow = curOrderSlotStart + m_cur.row;

    ImGuiListClipper clipper;
    clipper.Begin(totalRows);
    int slotIdx = 0;
    while (clipper.Step()) {
        // Walk slot pointer to the first slot covering DisplayStart
        while (slotIdx + 1 < (int)slots.size() && slots[slotIdx + 1].startAbs <= clipper.DisplayStart)
            ++slotIdx;

    for (int absR = clipper.DisplayStart; absR < clipper.DisplayEnd; absR++) {
        // Advance slot pointer as needed
        while (slotIdx + 1 < (int)slots.size() && slots[slotIdx + 1].startAbs <= absR)
            ++slotIdx;

        const Slot&   slot     = slots[slotIdx];
        const int32_t localR   = absR - slot.startAbs;
        const bool    isActive = (slot.orderIdx == curOrder);
        auto          dimIt    = m_slotDim.find(slot.orderIdx);
        const float   dimFactor = dimIt != m_slotDim.end() ? dimIt->second : (isActive ? 1.0f : 0.5f);

        bool isCur     = isActive && localR == curRow;
        bool isMeasure = (localR % slot.rpm == 0);
        bool isBeat    = (localR % slot.rpb == 0) && !isMeasure;

        ImGui::TableNextRow();

        if (isCur) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, kClrPatCurRowBg);
        } else if (isMeasure) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::ColorConvertFloat4ToU32(kBgMeasure));
        } else if (isBeat) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::ColorConvertFloat4ToU32(kBgHilight));
        }

        ImGui::TableNextColumn();
        {
            char rnBuf[8]; snprintf(rnBuf, sizeof(rnBuf), "%3d", localR);
            if (g_fontRowNum) ImGui::PushFont(g_fontRowNum);
            ImVec2 rnPos = ImGui::GetCursorScreenPos();
            float  rnW   = ImGui::CalcTextSize(rnBuf).x;
            rnPos.x = floorf(rnPos.x + m_rowNumW - rnW - 2.f);
            rnPos.y = floorf(rnPos.y) - 1.f;

            // Pattern boundary separator line
            if (m_continuousScroll && localR == 0 && absR > 0) {
                float winLeft  = ImGui::GetWindowPos().x;
                float winRight = winLeft + ImGui::GetWindowWidth();
                ImGui::GetWindowDrawList()->AddLine(
                    {winLeft, rnPos.y - 2.f}, {winRight, rnPos.y - 2.f},
                    IM_COL32(70, 70, 130, 220));
            }

            ImVec4 rnColV = isCur ? kTextCurRow : (isBeat||isMeasure ? kTextNormal : kDim);
            ImU32 rnCol = ImGui::ColorConvertFloat4ToU32(
                {rnColV.x * dimFactor, rnColV.y * dimFactor, rnColV.z * dimFactor, rnColV.w});
            ImGui::GetWindowDrawList()->AddText(rnPos, rnCol, rnBuf);
            float lineH = ImGui::GetTextLineHeight();
            if (g_fontRowNum) ImGui::PopFont();
            ImGui::Dummy({m_rowNumW, lineH});
        }

        for (int ch = 0; ch < nch; ch++) {
            ImGui::TableNextColumn();

            // Channel divider lines
            {
                ImVec2 cp   = ImGui::GetCursorScreenPos();
                float  padX = ImGui::GetStyle().CellPadding.x;
                float  padY = ImGui::GetStyle().CellPadding.y;
                float  rowH = ImGui::GetTextLineHeight() + 2.f * padY;
                float  top  = cp.y - padY;
                ImDrawList* sdl = ImGui::GetWindowDrawList();
                sdl->AddRectFilled({cp.x - padX, top}, {cp.x,               top + rowH}, IM_COL32(46, 46, 92, 200));
                sdl->AddRectFilled({cp.x + m_cellW, top}, {cp.x + m_cellW + padX, top + rowH}, IM_COL32(46, 46, 92, 200));
                sdl->AddLine({cp.x,               top}, {cp.x,               top + rowH}, IM_COL32(46, 46, 92, 200));
                sdl->AddLine({cp.x + m_cellW - 1, top}, {cp.x + m_cellW - 1, top + rowH}, IM_COL32(46, 46, 92, 200));
            }

            // Measure actual cell origins from the first rendered cell.
            if (absR == clipper.DisplayStart) {
                ImVec2 cp = ImGui::GetCursorScreenPos();
                if (ch == 0) {
                    m_tableOriginX = cp.x + m_hscroll;
                    m_tableOriginY = cp.y - float(absR) * m_rowH + m_scrollY
                                   - ImGui::GetStyle().CellPadding.y;
                } else if (ch == 1) {
                    m_colStep = (cp.x + m_hscroll) - m_tableOriginX;
                }
            }

            bool isEditCell = m_editMode && (absR == editAbsRow) && (ch == m_cur.channel);

            // Selection highlight — only within active pattern
            bool isSelected = isActive && m_sel.active
                && localR >= m_sel.rowMin() && localR <= m_sel.rowMax()
                && ch >= m_sel.chMin() && ch <= m_sel.chMax();
            if (isSelected) {
                ImVec2 rMin = ImGui::GetCursorScreenPos();
                ImVec2 rMax = {rMin.x + m_cellW, rMin.y + m_rowH};
                ImGui::GetWindowDrawList()->AddRectFilled(
                    rMin, rMax, IM_COL32(80, 80, 200, 60));
            }

            const bool showInst = m_detailView <= 2;
            const bool showVol  = m_detailView <= 1;
            const bool showFx   = m_detailView == 0;

            float subX[5] = {}, subW[5] = {};
            float cellY = 0.f;
            auto recordPos = [&](int col) {
                ImVec2 cp = ImGui::GetCursorScreenPos();
                subX[col] = cp.x + 0.5f;
                if (col == 0) cellY = cp.y;
            };

            std::string note  = mod->formatCellCommand(slot.pat, localR, ch, SoundModule::COMMAND_NOTE);
            std::string inst  = showInst ? mod->formatCellCommand(slot.pat, localR, ch, SoundModule::COMMAND_INSTRUMENT) : "";
            std::string fx    = showFx   ? mod->formatCellCommand(slot.pat, localR, ch, SoundModule::COMMAND_EFFECT)     : "";
            std::string fxp   = showFx   ? mod->formatCellCommand(slot.pat, localR, ch, SoundModule::COMMAND_PARAMETER)  : "";

            // libopenmpt formats vol as hex; read raw cell and format as decimal to match OpenMPT
            PatternCell rawCell{};
            if (showVol) pe_get_cell(mod, slot.pat, localR, ch, rawCell);
            bool hasVol = showVol && rawCell.volcmd != VOLCMD_NONE;
            char volBuf[8] = {};
            if (hasVol) {
                static constexpr char kVolCmdLetters[] = " vpcdabuhlrgfe:o";
                char letter = (rawCell.volcmd < 16) ? kVolCmdLetters[rawCell.volcmd] : '?';
                snprintf(volBuf, sizeof(volBuf), "%c%02d", letter, (int)rawCell.vol);
            }
            std::string vol = hasVol ? volBuf : "";

            bool hasNote = !note.empty() && note != "..." && note != "---";
            bool hasInst = showInst && !inst.empty() && inst != ".." && inst != "00";
            bool hasFx   = showFx && !fx.empty() && fx != ".";

            float cellDim = isActive ? ((m_muteState >> ch) & 1 ? 0.5f : 1.0f) : dimFactor;
            auto dimColor = [cellDim](const ImVec4& c) -> ImVec4 {
                return {c.x * cellDim, c.y * cellDim, c.z * cellDim, c.w};
            };

            {
                ImVec2 cur = ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos({cur.x + 4.f, cur.y});
            }
            recordPos(0);
            ImGui::TextColored(dimColor(hasNote ? kColNote : kDim), "%-3s", hasNote ? note.c_str() : "...");
            subW[0] = ImGui::GetItemRectSize().x;
            if (showInst) {
                ImGui::SameLine(0, 6); recordPos(1);
                ImGui::TextColored(dimColor(hasInst ? kColInstr : kDim), "%-2s", hasInst ? inst.c_str() : "..");
                subW[1] = ImGui::GetItemRectSize().x;
            }
            if (showVol) {
                ImGui::SameLine(0, 6); recordPos(2);
                ImGui::TextColored(dimColor(hasVol ? kColVolume : kDim), "%-3s", hasVol ? vol.c_str() : "...");
                subW[2] = ImGui::GetItemRectSize().x;
            }
            if (showFx) {
                ImGui::SameLine(0, 6); recordPos(3);
                ImGui::TextColored(dimColor(hasFx ? kColGlobal : kDim), "%s",   hasFx ? fx.c_str()  : ".");
                subW[3] = ImGui::GetItemRectSize().x;
                ImGui::SameLine(0, 0); recordPos(4);
                ImGui::TextColored(dimColor(hasFx ? kColGlobal : kDim), "%-2s", hasFx ? fxp.c_str() : "..");
                subW[4] = ImGui::GetItemRectSize().x;
            }

            if (isEditCell) {
                int lastCol = showFx ? 4 : (showVol ? 2 : (showInst ? 1 : 0));
                int col = std::min(int(m_cur.col), lastCol);
                float y0 = ImGui::GetItemRectMin().y;
                float y1 = ImGui::GetItemRectMax().y;
                ImU32 hilite = IM_COL32(255, 220, 80, 80);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {subX[col]-1, y0}, {subX[col]+subW[col], y1+1}, hilite);
                float x0cell = subX[0] - 1;
                float x1cell = subX[lastCol] + subW[lastCol] + 1;
                ImGui::GetWindowDrawList()->AddRect(
                    {x0cell, y0-1}, {x1cell, y1+1}, IM_COL32(255,255,255,220));
            }

            // Glow on non-empty cells — active pattern, unmuted channels only
            bool chMuted = (m_muteState >> ch) & 1;
            if (isActive && !chMuted && g_glowPattern > 0.f && (hasNote || hasInst || hasVol || hasFx)) {
                ImDrawList* gdl  = ImGui::GetWindowDrawList();
                ImFont*     font = ImGui::GetFont();
                float       fsz  = ImGui::GetFontSize();
                gdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                }, nullptr);
                auto addGlowText = [&](float gx, const ImVec4& c, const char* txt) {
                    uint8_t a1 = (uint8_t)((isCur ? 25 : 12) * g_glowPattern);
                    uint8_t a2 = (uint8_t)((isCur ? 12 :  7) * g_glowPattern);
                    uint8_t a3 = (uint8_t)((isCur ?  9 :  0) * g_glowPattern);
                    ImU32 gc1 = IM_COL32((uint8_t)(c.x*255.f),(uint8_t)(c.y*255.f),(uint8_t)(c.z*255.f), a1);
                    ImU32 gc2 = IM_COL32((uint8_t)(c.x*255.f),(uint8_t)(c.y*255.f),(uint8_t)(c.z*255.f), a2);
                    ImU32 gc3 = IM_COL32((uint8_t)(c.x*255.f),(uint8_t)(c.y*255.f),(uint8_t)(c.z*255.f), a3);
                    gdl->AddText(font, fsz, {gx-1, cellY-1}, gc1, txt);
                    gdl->AddText(font, fsz, {gx+1, cellY-1}, gc1, txt);
                    gdl->AddText(font, fsz, {gx-1, cellY+1}, gc1, txt);
                    gdl->AddText(font, fsz, {gx+1, cellY+1}, gc1, txt);
                    gdl->AddText(font, fsz, {gx-2, cellY-2}, gc2, txt);
                    gdl->AddText(font, fsz, {gx+2, cellY-2}, gc2, txt);
                    gdl->AddText(font, fsz, {gx-2, cellY+2}, gc2, txt);
                    gdl->AddText(font, fsz, {gx+2, cellY+2}, gc2, txt);
                    if (a3) {
                        gdl->AddText(font, fsz, {gx-3, cellY-3}, gc3, txt);
                        gdl->AddText(font, fsz, {gx+3, cellY-3}, gc3, txt);
                        gdl->AddText(font, fsz, {gx-3, cellY+3}, gc3, txt);
                        gdl->AddText(font, fsz, {gx+3, cellY+3}, gc3, txt);
                    }
                };
                if (hasNote)              addGlowText(subX[0], kColNote,   note.c_str());
                if (hasInst)              addGlowText(subX[1], kColInstr,  inst.c_str());
                if (hasVol)               addGlowText(subX[2], kColVolume, vol.c_str());
                if (hasFx) {
                    addGlowText(subX[3], kColGlobal, fx.c_str());
                    addGlowText(subX[4], kColGlobal, fxp.c_str());
                }
                gdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }, nullptr);
            }
        }
    }
    }
    clipper.End();
    if (m_fontMono) ImGui::PopFont();
    ImGui::EndTable();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

}

