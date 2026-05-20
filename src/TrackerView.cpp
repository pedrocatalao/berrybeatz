#include "TrackerView.h"
#include "PatternEditor.h"
#include <libopenmpt/libopenmpt_ext.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>

// ── Note key table (FT2-style piano layout) ───────────────────────────────────
struct NoteKey { ImGuiKey key; int semi; };
static constexpr NoteKey kNoteKeys[] = {
    // Lower row → base octave
    {ImGuiKey_Z,0},{ImGuiKey_S,1},{ImGuiKey_X,2},{ImGuiKey_D,3},
    {ImGuiKey_C,4},{ImGuiKey_V,5},{ImGuiKey_G,6},{ImGuiKey_B,7},
    {ImGuiKey_H,8},{ImGuiKey_N,9},{ImGuiKey_J,10},{ImGuiKey_M,11},
    // Upper row → base+1 octave
    {ImGuiKey_Q,12},{ImGuiKey_2,13},{ImGuiKey_W,14},{ImGuiKey_3,15},
    {ImGuiKey_E,16},{ImGuiKey_R,17},{ImGuiKey_5,18},{ImGuiKey_T,19},
    {ImGuiKey_6,20},{ImGuiKey_Y,21},{ImGuiKey_7,22},{ImGuiKey_U,23},
};
static constexpr NoteKey kHexKeys[] = {
    {ImGuiKey_0,0},{ImGuiKey_1,1},{ImGuiKey_2,2},{ImGuiKey_3,3},
    {ImGuiKey_4,4},{ImGuiKey_5,5},{ImGuiKey_6,6},{ImGuiKey_7,7},
    {ImGuiKey_8,8},{ImGuiKey_9,9},
    {ImGuiKey_A,10},{ImGuiKey_B,11},{ImGuiKey_C,12},
    {ImGuiKey_D,13},{ImGuiKey_E,14},{ImGuiKey_F,15},
};

// ── OpenMPT "Blue (FT2)" colour scheme ────────────────────────────────────────
static const ImVec4 kBgNormal    { 20/255.f,  20/255.f,  33/255.f, 1.f};
static const ImVec4 kTextNormal  {224/255.f, 224/255.f,  64/255.f, 1.f};
static const ImVec4 kBgCurRow    { 40/255.f,  85/255.f,  145/255.f, 0.85f};
static const ImVec4 kTextCurRow  {240/255.f, 240/255.f,  80/255.f, 1.f};
static const ImVec4 kBgHilight   { 64/255.f,  64/255.f, 128/255.f, 1.f};
static const ImVec4 kBgMeasure   { 46/255.f,  46/255.f,  90/255.f, 1.f};
static const ImVec4 kColNote     {100/255.f, 210/255.f, 220/255.f, 1.f};  // soft cyan
static const ImVec4 kColInstr    {220/255.f, 200/255.f, 110/255.f, 1.f};  // soft yellow
static const ImVec4 kColVolume   {220/255.f, 130/255.f, 160/255.f, 1.f};  // soft pink
static const ImVec4 kColGlobal   {120/255.f, 200/255.f, 130/255.f, 1.f};  // soft green
static const ImVec4 kDim         { 80/255.f,  80/255.f,  80/255.f, 1.f};
static const ImVec4 kChHeader    { 30/255.f,  30/255.f,  60/255.f, 1.f};
static const ImVec4 kOrderBg     {  0/255.f,   0/255.f,   0/255.f, 0.f};
static const ImVec4 kOrderCur    { 64/255.f,  64/255.f, 160/255.f, 1.f};

static const ImU32 kVuLoLit    = IM_COL32(  0, 200,   0, 255);
static const ImU32 kVuLoUnlit  = IM_COL32(  0,  28,   0, 255);
static const ImU32 kVuMedLit   = IM_COL32(255, 200,   0, 255);
static const ImU32 kVuMedUnlit = IM_COL32( 40,  30,   0, 255);
static const ImU32 kVuHiLit    = IM_COL32(225,   0,   0, 255);
static const ImU32 kVuHiUnlit  = IM_COL32( 35,   0,   0, 255);

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr float PTBAR1_H   = 32.0f;
static constexpr float PTBAR2_H   = 30.0f;
static constexpr float ORDER_H    = 26.0f;
static constexpr float CHANNAME_H = 18.0f;
static constexpr float VU_H       = 13.0f;
static constexpr float WAVE_H     = 28.0f;
static constexpr float PLUGIN_H   = 14.0f;

// ── TrackerView ───────────────────────────────────────────────────────────────

TrackerView::TrackerView(ModuleState& state) : m_state(state) {}

// ── Undo / Redo ───────────────────────────────────────────────────────────────

void TrackerView::pushEdit(UndoEntry e) {
    m_redoStack.clear();
    m_undoStack.push_back(std::move(e));
    if ((int)m_undoStack.size() > kMaxUndo)
        m_undoStack.pop_front();
}

void TrackerView::doUndo() {
    if (m_undoStack.empty() || !m_mod) return;
    UndoEntry e = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    for (auto& c : e)
        pe_set_cell(m_mod, c.pat, c.row, c.ch, c.before);
    m_redoStack.push_back(std::move(e));
}

void TrackerView::doRedo() {
    if (m_redoStack.empty() || !m_mod) return;
    UndoEntry e = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    for (auto& c : e)
        pe_set_cell(m_mod, c.pat, c.row, c.ch, c.after);
    m_undoStack.push_back(std::move(e));
}

// Helper: perform a single-cell edit with undo tracking
void TrackerView::setCell(int32_t pat, int32_t row, int32_t ch, const PatternCell& cell) {
    PatternCell before;
    pe_get_cell(m_mod, pat, row, ch, before);
    pe_set_cell(m_mod, pat, row, ch, cell);
    UndoEntry e;
    e.push_back({pat, row, ch, before, cell});
    pushEdit(std::move(e));
}

// ── Block operations ──────────────────────────────────────────────────────────

void TrackerView::selCopy() {
    if (!m_sel.active || !m_mod) return;
    int32_t pat  = m_state.current_pattern.load();
    int32_t rMin = m_sel.rowMin(), rMax = m_sel.rowMax();
    int32_t cMin = m_sel.chMin(),  cMax = m_sel.chMax();
    m_clip.rows  = rMax - rMin + 1;
    m_clip.chans = cMax - cMin + 1;
    m_clip.cells.assign(m_clip.rows, std::vector<PatternCell>(m_clip.chans));
    for (int32_t r = 0; r < m_clip.rows; r++)
        for (int32_t c = 0; c < m_clip.chans; c++)
            pe_get_cell(m_mod, pat, rMin + r, cMin + c, m_clip.cells[r][c]);
}

void TrackerView::selPaste() {
    if (m_clip.rows <= 0 || m_clip.chans <= 0 || !m_mod) return;
    int32_t pat   = m_state.current_pattern.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    int32_t nch   = m_state.num_channels.load();
    UndoEntry e;
    for (int32_t r = 0; r < m_clip.rows; r++) {
        int32_t dr = m_cur.row + r;
        if (dr >= nrows) { if (!m_overflowPaste) break; dr %= nrows; }
        for (int32_t c = 0; c < m_clip.chans; c++) {
            int32_t dc = m_cur.channel + c;
            if (dc >= nch) break;
            PatternCell before;
            pe_get_cell(m_mod, pat, dr, dc, before);
            pe_set_cell(m_mod, pat, dr, dc, m_clip.cells[r][c]);
            e.push_back({pat, dr, dc, before, m_clip.cells[r][c]});
        }
    }
    if (!e.empty()) pushEdit(std::move(e));
}

void TrackerView::selDelete() {
    if (!m_sel.active || !m_mod) return;
    int32_t pat  = m_state.current_pattern.load();
    int32_t rMin = m_sel.rowMin(), rMax = m_sel.rowMax();
    int32_t cMin = m_sel.chMin(),  cMax = m_sel.chMax();
    UndoEntry e;
    for (int32_t r = rMin; r <= rMax; r++) {
        for (int32_t c = cMin; c <= cMax; c++) {
            PatternCell before;
            pe_get_cell(m_mod, pat, r, c, before);
            static const PatternCell kEmpty{};
            pe_set_cell(m_mod, pat, r, c, kEmpty);
            e.push_back({pat, r, c, before, kEmpty});
        }
    }
    if (!e.empty()) pushEdit(std::move(e));
}

void TrackerView::insertRow() {
    if (!m_mod) return;
    int32_t pat  = m_state.current_pattern.load();
    int32_t nch  = m_state.num_channels.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    // Build undo entry: snapshot before state
    UndoEntry e;
    e.reserve((nrows - m_cur.row) * nch);
    for (int32_t r = m_cur.row; r < nrows; r++) {
        for (int32_t c = 0; c < nch; c++) {
            PatternCell before;
            pe_get_cell(m_mod, pat, r, c, before);
            e.push_back({pat, r, c, before, PatternCell{}});
        }
    }
    // Perform the shift
    pe_insert_row(m_mod, pat, m_cur.row, nch);
    // Capture after state
    for (auto& ce : e) {
        pe_get_cell(m_mod, pat, ce.row, ce.ch, ce.after);
    }
    pushEdit(std::move(e));
}

void TrackerView::deleteRow() {
    if (!m_mod) return;
    int32_t pat  = m_state.current_pattern.load();
    int32_t nch  = m_state.num_channels.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    // Snapshot before state
    UndoEntry e;
    e.reserve((nrows - m_cur.row) * nch);
    for (int32_t r = m_cur.row; r < nrows; r++) {
        for (int32_t c = 0; c < nch; c++) {
            PatternCell before;
            pe_get_cell(m_mod, pat, r, c, before);
            e.push_back({pat, r, c, before, PatternCell{}});
        }
    }
    // Perform the shift
    pe_delete_row(m_mod, pat, m_cur.row, nch);
    // Capture after state
    for (auto& ce : e) {
        pe_get_cell(m_mod, pat, ce.row, ce.ch, ce.after);
    }
    pushEdit(std::move(e));
}

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
    m_rowNumW = m_charW * 3.0f + 2.0f;
    m_cellW   = m_charW * 11.0f + 26.0f;
    m_rowH    = ImGui::GetTextLineHeightWithSpacing();
    if (m_fontMono) ImGui::PopFont();

    m_chanH  = CHANNAME_H
             + (m_showVU        ? VU_H     : 0.f)
             + (m_showWave      ? WAVE_H   : 0.f)
             + (m_showPluginRow ? PLUGIN_H : 0.f);
    m_patTop = y + PTBAR1_H + PTBAR2_H + ORDER_H + m_chanH;

    ImGuiWindowFlags noDecoFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    const ImVec4 kBarBg{10/255.f, 10/255.f, 20/255.f, 1.f};

    // Pattern toolbar row 1
    ImGui::SetNextWindowPos ({x, y});
    ImGui::SetNextWindowSize({w, PTBAR1_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBarBg);
    ImGui::Begin("##ptbar1", nullptr, noDecoFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawPatternToolbar1(x, y, w);
    ImGui::End();

    // Pattern toolbar row 2
    ImGui::SetNextWindowPos ({x, y + PTBAR1_H});
    ImGui::SetNextWindowSize({w, PTBAR2_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBarBg);
    ImGui::Begin("##ptbar2", nullptr, noDecoFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawPatternToolbar2(x, y + PTBAR1_H, w);
    ImGui::End();

    // Order list
    ImGui::SetNextWindowPos ({x, y + PTBAR1_H + PTBAR2_H});
    ImGui::SetNextWindowSize({w, ORDER_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kOrderBg);
    ImGui::Begin("##orders", nullptr, noDecoFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawOrderList(x, y + PTBAR1_H + PTBAR2_H, w);
    ImGui::End();

    // Pattern panel — channel headers are the frozen first row of the table
    float chanY = y + PTBAR1_H + PTBAR2_H + ORDER_H;
    float patH  = h - (chanY - y);
    ImGui::SetNextWindowPos ({x, chanY});
    ImGui::SetNextWindowSize({w, patH});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,             kBgNormal);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          IM_COL32( 12,  12,  22, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        IM_COL32( 40,  85, 145, 200));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32( 55, 112, 175, 230));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  IM_COL32( 66, 130, 205, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##pattern", nullptr, noDecoFlags);
    ImGui::PopStyleVar(1);   // WindowPadding — no longer needed after Begin
    ImGui::PopStyleColor(1); // WindowBg — no longer needed after Begin

    if (!ImGui::GetIO().WantTextInput)
        processInput();

    drawPatternPanel(x, chanY, w, patH);
    ImGui::End();
    ImGui::PopStyleColor(4); // scrollbar colors — must outlive End()

    // Plugin settings popup (anchored to chanstrip window for ID stability)
    ImGui::SetNextWindowPos({x, chanY});
    ImGui::SetNextWindowSize({1, 1}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##plughost", nullptr, noDecoFlags | ImGuiWindowFlags_NoNav);
    if (m_pluginPopupCh >= 0) {
        ImGui::OpenPopup("##plugsettings");
        m_pluginPopupCh = -1;
    }
    ImGui::SetNextWindowSize({300, 0}, ImGuiCond_Always);
    if (ImGui::BeginPopupModal("##plugsettings", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextColored(kTextNormal, "Channel — Plugin Settings");
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(kDim, "No plugin assigned.");
        ImGui::Spacing(); ImGui::Separator();
        if (ImGui::Button("Close", {80, 0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

// ── Pattern toolbar row 1 ─────────────────────────────────────────────────────

void TrackerView::drawPatternToolbar1(float /*x*/, float /*y*/, float /*w*/) {
    bool has = m_mod != nullptr;
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
    if (actBtn("Record##rec", has)) m_editMode = !m_editMode;
    if (wasRec) ImGui::PopStyleColor(2);
    tip("Toggle record mode (Enter)");

    tbSep();
    togBtn("VU##vu", m_showVU);   tip("Show/hide channel VU meters");
    ImGui::SameLine();
    togBtn("WF##wf", m_showWave); tip("Show/hide channel waveform");
    ImGui::SameLine();
    togBtn("FX##fx", m_showPluginRow); tip("Show/hide plugin row");

    tbSep();
    if (actBtn("Undo##undo", has && !m_undoStack.empty())) { doUndo(); } tip("Undo");
    ImGui::SameLine();
    if (actBtn("Props##props",     has)) {} tip("Pattern properties");
    ImGui::SameLine();
    if (actBtn("Expand##exp",      has)) {} tip("Expand pattern");
    ImGui::SameLine();
    if (actBtn("Shrink##shr",      has)) {} tip("Shrink pattern");

    tbSep();
    togBtn("Detail##detail", m_detailView); tip("Detail view");
    tbSep();
    togBtn("Overflow##ovf", m_overflowPaste); tip("Overflow paste");

    popToolbarStyle();
    if (m_fontUI) ImGui::PopFont();
}

// ── Pattern toolbar row 2 ─────────────────────────────────────────────────────

void TrackerView::drawPatternToolbar2(float /*x*/, float /*y*/, float /*w*/) {
    bool has = m_mod != nullptr;
    if (m_fontUI) ImGui::PushFont(m_fontUI);
    pushToolbarStyle();
    ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) * 0.5f);

    ImGui::TextColored(kDim, "Instr");
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(48.0f);
    if (ImGui::InputInt("##instr", &m_curInstrument, 1, 1))
        m_curInstrument = std::clamp(m_curInstrument, 1, 255);
    tip("Current instrument number");

    tbSep();

    ImGui::TextColored(kDim, "Plugin");
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(70.0f);
    ImGui::BeginDisabled();
    int plugDummy = 0;
    ImGui::InputInt("##plugin2", &plugDummy, 0, 0);
    ImGui::EndDisabled();
    tip("Plugin selector (not yet available)");

    tbSep();

    ImGui::TextColored(kDim, "Step");
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(44.0f);
    if (ImGui::InputInt("##step2", &m_editStep, 1, 1))
        m_editStep = std::clamp(m_editStep, 0, 16);
    tip("Edit step (- / + keys)");

    tbSep();
    togBtn("Loop##lp",   m_loopPattern); tip("Loop current pattern");
    ImGui::SameLine();
    togBtn("Follow##fw", m_followSong);  tip("Follow playback cursor");

    tbSep();

    ImGui::TextColored(kDim, "Pattern");
    ImGui::SameLine(0, 4);
    int32_t curPat = m_mod ? m_state.current_pattern.load() : -1;
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

    if (norders <= 0) {
        ImGui::SetCursorPosY(4);
        ImGui::TextColored(kDim, "  Open a module file  (File > Open or drag & drop)");
        return;
    }

    float boxW = m_charW * 4.2f;
    float boxH = ORDER_H - 4.0f;

    ImGui::SetCursorPosY(2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   {2.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  {2.0f, 1.0f});

    for (int32_t i = 0; i < norders; i++) {
        bool isCur  = (i == curOrder);
        ImVec4 btnBg = isCur ? kOrderCur : kOrderBg;
        ImGui::PushStyleColor(ImGuiCol_Button,        btnBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {btnBg.x+.1f, btnBg.y+.1f, btnBg.z+.2f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {btnBg.x+.2f, btnBg.y+.2f, btnBg.z+.3f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,          isCur ? kTextCurRow : kTextNormal);

        int32_t pat = m_mod ? m_mod->get_order_pattern(i) : i;
        char label[12];
        snprintf(label, sizeof(label), "%02d##o%d", pat, i);
        if (ImGui::Button(label, {boxW, boxH}))
            if (onOrderSeekRequested) onOrderSeekRequested(i);

        ImGui::PopStyleColor(4);
        ImGui::SameLine(0, 2);
    }

    ImGui::PopStyleVar(3);
}

// ── Stereo VU ─────────────────────────────────────────────────────────────────

void TrackerView::drawStereoVU(ImDrawList* dl, float x, float y, float w, float h,
                                float lvlL, float lvlR) {
    constexpr float barW   = 2.0f;
    constexpr float stride = 3.0f;
    float cx  = x + w * 0.5f;
    int   N   = std::max(1, int((w * 0.5f - 1.0f) / stride));
    float bH  = h - 1.0f;
    float normL = std::min(lvlL / 1.2f, 1.0f);
    float normR = std::min(lvlR / 1.2f, 1.0f);

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

// ── Channel strip ─────────────────────────────────────────────────────────────

void TrackerView::drawChannelStrip(float /*x*/, float /*y*/, float /*w*/) {
    int32_t nch = m_state.num_channels.load();
    if (nch <= 0) return;

    std::array<ChannelVU, MAX_CHANNELS> vu{};
    { std::lock_guard<std::mutex> lk(m_state.vu_mutex); vu = m_state.vu; }

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

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      wPos = ImGui::GetWindowPos();

    {
        float textH    = ImGui::GetTextLineHeight();
        float rowLabelY = wPos.y + (m_chanH - textH) * 0.5f;
        dl->AddText({wPos.x + 6, rowLabelY},
                    ImGui::ColorConvertFloat4ToU32(kDim), "Row");
    }

    float scrollX  = m_hscroll;
    float cellW2   = m_cellW;
    float chStart  = m_rowNumW - scrollX;

    uint64_t newMuteState = m_muteState;
    int32_t plugClickCh = -1;

    for (int ch = 0; ch < nch; ch++) {
        float chX = wPos.x + chStart + float(ch) * cellW2;
        if (chX + cellW2 < wPos.x || chX > wPos.x + ImGui::GetWindowWidth()) continue;

        bool muted = (m_muteState >> ch) & 1;

        // Channel name bar
        ImU32 hdrCol = muted ? IM_COL32(20,20,30,255) : IM_COL32(30,30,60,255);
        dl->AddRectFilled({chX, wPos.y}, {chX + cellW2 - 1, wPos.y + CHANNAME_H}, hdrCol);
        dl->AddLine({chX + cellW2 - 1, wPos.y}, {chX + cellW2 - 1, wPos.y + m_chanH},
                    IM_COL32(46, 46, 92, 200));

        float textH = ImGui::GetTextLineHeight();
        float textY = wPos.y + (CHANNAME_H - textH) * 0.5f;
        ImU32 textCol = muted ? IM_COL32(60,60,60,255) : IM_COL32(200,200,120,255);
        std::string label = chNames[ch];
        ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        float tx = chX + (cellW2 - ts.x) * 0.5f;
        dl->AddText({tx, textY}, textCol, label.c_str());

        // Mute click
        ImGui::SetCursorScreenPos({chX, wPos.y});
        ImGui::PushID(ch);
        if (ImGui::InvisibleButton("##mute", {cellW2, CHANNAME_H})) {
            newMuteState ^= (uint64_t(1) << ch);
        }

        // VU
        float vuTop = wPos.y + CHANNAME_H;
        if (m_showVU) {
            drawStereoVU(dl, chX, vuTop, cellW2 - 1, VU_H, vu[ch].left, vu[ch].right);
        }

        // Plugin row
        float vuBottom = vuTop + (m_showVU ? VU_H : 0.f);
        if (m_showPluginRow) {
            ImU32 plugBg = IM_COL32(20, 20, 35, 255);
            dl->AddRectFilled({chX, vuBottom}, {chX + cellW2 - 1, vuBottom + PLUGIN_H}, plugBg);
            dl->AddText({chX + 2, vuBottom + 1}, IM_COL32(60, 60, 80, 255), "–");

            ImGui::SetCursorScreenPos({chX, vuBottom});
            if (ImGui::InvisibleButton("##plug", {cellW2, PLUGIN_H}))
                plugClickCh = ch;
        }

        ImGui::PopID();
    }

    if (newMuteState != m_muteState) {
        m_muteState = newMuteState;
        m_state.channel_mute_mask.store(newMuteState);
    }

    if (plugClickCh >= 0) {
        m_pluginPopupCh = plugClickCh;
    }
}

// ── Pattern panel ─────────────────────────────────────────────────────────────

void TrackerView::drawPatternPanel(float /*x*/, float /*y*/, float /*w*/, float /*h*/) {
    if (!m_mod || m_state.num_channels.load() <= 0) {
        ImGui::TextColored(kDim, " No module loaded.");
        return;
    }

    // Handle click to move edit cursor.
    // Uses m_tableOriginX/Y measured from inside the table loop last frame,
    // which correctly accounts for table cell padding.
    if (ImGui::IsMouseClicked(0) && m_rowH > 0.f && m_tableOriginY > 0.f) {
        ImVec2 mp = ImGui::GetIO().MousePos;
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        bool inGrid = ImGui::IsMouseHoveringRect(
            {wp.x, wp.y + m_chanH}, {wp.x + ws.x, wp.y + ws.y}, false);
        if (inGrid) {
            float relY = mp.y - m_tableOriginY + m_scrollY;
            float step  = m_colStep > 0.f ? m_colStep : (m_cellW + 2.f * ImGui::GetStyle().CellPadding.x);
            float relX  = mp.x - (m_tableOriginX - m_hscroll);
            int32_t nch   = m_state.num_channels.load();
            int32_t nrows = pe_num_rows(m_mod, m_state.current_pattern.load());
            int row = int(relY / m_rowH);
            int ch  = int(relX / step);
            if (row >= 0 && row < nrows && ch >= 0 && ch < nch) {
                m_cur.row = row; m_cur.channel = ch; m_cur.col = EditCol::Note;
                m_editMode = true;
            }
        }
    }

    int32_t curPat  = m_state.current_pattern.load();
    int32_t curRow  = m_state.current_row.load();
    int32_t nch     = m_state.num_channels.load();
    int32_t nrows   = m_mod->get_pattern_num_rows(curPat);

    int rpb = m_mod->get_pattern_rows_per_beat(curPat);
    if (rpb <= 0) rpb = 4;
    int rpm = rpb * 4;

    int totalCols = 1 + nch;
    ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX;

    float rowH  = m_rowH;  // pre-computed with mono font in render()
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float visH  = avail.y - m_chanH;  // exclude frozen header row from scroll math

    // Fetch channel data for the frozen header row
    std::array<ChannelVU, MAX_CHANNELS> vu{};
    WaveSnapshot masterWave{};
    {
        std::lock_guard<std::mutex> lk(m_state.vu_mutex);
        vu         = m_state.vu;
        masterWave = m_state.masterWave;
    }
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
    bool tableOpen = ImGui::BeginTable("##pat", totalCols, tFlags, avail);
    if (!tableOpen) { ImGui::PopStyleColor(2); return; }

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
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(20, 20, 40, 255), 0);

        // Column 0: "Row" label
        ImGui::TableSetColumnIndex(0);
        {
            ImVec2 p    = ImGui::GetCursorScreenPos();
            float textH = ImGui::GetTextLineHeight();
            dl->AddText({p.x + 2, p.y + (m_chanH - textH) * 0.5f},
                        ImGui::ColorConvertFloat4ToU32(kDim), "Row");
            ImGui::Dummy({m_rowNumW, m_chanH});
        }

        for (int ch = 0; ch < nch; ch++) {
            ImGui::TableSetColumnIndex(ch + 1);
            ImVec2 p  = ImGui::GetCursorScreenPos();
            float  cw = m_cellW;
            bool muted = (m_muteState >> ch) & 1;

            // Background + divider
            ImU32 hdrCol = muted ? IM_COL32(20,20,30,255) : IM_COL32(30,30,60,255);
            dl->AddRectFilled(p, {p.x + cw - 1, p.y + CHANNAME_H}, hdrCol);
            dl->AddLine({p.x + cw - 1, p.y}, {p.x + cw - 1, p.y + m_chanH},
                        IM_COL32(46, 46, 92, 200));

            // Channel name, centred
            float textH = ImGui::GetTextLineHeight();
            ImU32 textCol = muted ? IM_COL32(60,60,60,255) : IM_COL32(200,200,120,255);
            ImVec2 ts = ImGui::CalcTextSize(chNames[ch].c_str());
            dl->AddText({p.x + (cw - ts.x) * 0.5f, p.y + (CHANNAME_H - textH) * 0.5f},
                        textCol, chNames[ch].c_str());

            // Mute click target
            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(0x10000 + ch);
            if (ImGui::InvisibleButton("##mute", {cw, CHANNAME_H}))
                newMuteState ^= (uint64_t(1) << ch);

            // VU
            float vuTop = p.y + CHANNAME_H;
            if (m_showVU)
                drawStereoVU(dl, p.x, vuTop, cw - 1, VU_H, vu[ch].left, vu[ch].right);

            // Oscilloscope waveform
            float vuBot = vuTop + (m_showVU ? VU_H : 0.f);
            if (m_showWave && masterWave.count > 1) {
                float wy    = vuBot;
                dl->AddRectFilled({p.x, wy}, {p.x + cw - 1, wy + WAVE_H}, IM_COL32(20, 20, 33, 255));
                float cy    = wy + WAVE_H * 0.5f;
                float halfH = WAVE_H * 0.5f - 2.f;

                // Show only a portion of the buffer so individual waves are visible
                static constexpr int kDisplaySamples = 160;
                int total = masterWave.count;
                int displayN = std::min(total, kDisplaySamples);

                // Oscilloscope trigger: find first upward zero crossing in the
                // lead-in window so the waveform appears stable (no flickering)
                int start = 0;
                int searchEnd = total - displayN;
                for (int i = 1; i < searchEnd; ++i) {
                    if (masterWave.samples[i - 1] <= 0.f && masterWave.samples[i] > 0.f) {
                        start = i;
                        break;
                    }
                }

                // Normalize by peak, partial floor so quiet signals stay readable
                float peak = 0.f;
                for (int i = start; i < start + displayN; ++i)
                    peak = std::max(peak, std::abs(masterWave.samples[i]));
                float norm = (peak > 0.0001f) ? halfH / std::max(peak, 0.3f) : 0.f;
                // Fade when channel is silent
                float chAmp = (vu[ch].left + vu[ch].right) * 0.5f;
                norm *= std::min(chAmp * 30.0f, 1.0f);

                ImU32 wavCol = muted ? IM_COL32(40, 55, 45, 160) : IM_COL32(70, 215, 120, 220);
                float xscale = (cw - 2.f) / float(displayN);
                ImVec2 prev  = {p.x, cy - masterWave.samples[start] * norm};
                for (int i = 1; i < displayN; ++i) {
                    float fx = p.x + float(i) * xscale;
                    float fy = cy - masterWave.samples[start + i] * norm;
                    dl->AddLine(prev, {fx, fy}, wavCol);
                    prev = {fx, fy};
                }
            }

            // Plugin row
            float plugTop = vuBot + (m_showWave ? WAVE_H : 0.f);
            if (m_showPluginRow) {
                dl->AddRectFilled({p.x, plugTop}, {p.x + cw - 1, plugTop + PLUGIN_H},
                                  IM_COL32(20, 20, 35, 255));
                dl->AddText({p.x + 2, plugTop + 1}, IM_COL32(60, 60, 80, 255), "–");
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
    bool  playing    = m_state.is_playing.load();
    bool  patChanged = (curPat != m_lastPattern);
    bool  rowRewound = (playing && !m_editMode && curRow < m_lastRow - 2);

    bool  forceScroll = false;
    float newScrollY  = currentScrollY;

    if (patChanged || rowRewound) {
        int32_t snapRow = m_editMode ? m_cur.row : curRow;
        newScrollY  = float(snapRow) * rowH - visH * 0.5f + rowH * 0.5f;
        forceScroll = true;
    } else if (playing && !m_editMode && m_followSong) {
        float targetY = float(curRow) * rowH - visH * 0.5f + rowH * 0.5f;
        float dt = ImGui::GetIO().DeltaTime;
        newScrollY  = currentScrollY + (targetY - currentScrollY) * std::min(1.0f, 8.0f * dt);
        forceScroll = true;
    }

    if (forceScroll) {
        newScrollY = std::max(0.f, std::min(newScrollY, float(nrows) * rowH));
        ImGui::SetScrollY(newScrollY);
        m_scrollY = newScrollY;
    } else {
        m_scrollY = currentScrollY;
    }

    m_lastPattern = curPat;
    m_lastRow     = curRow;

    if (m_fontMono) ImGui::PushFont(m_fontMono);

    ImGuiListClipper clipper;
    clipper.Begin(nrows);
    while (clipper.Step()) {
    for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++) {
        ImGui::TableNextRow();
        bool isCur     = (r == curRow);
        bool isMeasure = (r % rpm == 0);
        bool isBeat    = (r % rpb == 0) && !isMeasure;

        if (isCur)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(kBgCurRow));
        else if (isMeasure)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(kBgMeasure));
        else if (isBeat)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(kBgHilight));

        if (!isCur) {
            ImU32 rowNumBg = isMeasure ? ImGui::ColorConvertFloat4ToU32(kBgMeasure)
                           : isBeat    ? ImGui::ColorConvertFloat4ToU32(kBgHilight)
                           : IM_COL32(20, 20, 40, 255);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, rowNumBg, 0);
        }

        ImGui::TableNextColumn();
        ImGui::TextColored(isCur ? kTextCurRow : (isBeat||isMeasure ? kTextNormal : kDim), "%3d", r);

        for (int ch = 0; ch < nch; ch++) {
            ImGui::TableNextColumn();

            // Measure actual cell origins from the first two rendered cells.
            if (r == clipper.DisplayStart) {
                ImVec2 cp = ImGui::GetCursorScreenPos();
                if (ch == 0) {
                    m_tableOriginX = cp.x + m_hscroll;
                    m_tableOriginY = cp.y - float(r) * m_rowH + m_scrollY
                                   - ImGui::GetStyle().CellPadding.y;
                } else if (ch == 1) {
                    m_colStep = (cp.x + m_hscroll) - m_tableOriginX;
                }
            }

            bool isEditCell = m_editMode && (r == m_cur.row) && (ch == m_cur.channel);

            // Selection highlight (tinted background, drawn before text)
            bool isSelected = m_sel.active
                && r >= m_sel.rowMin() && r <= m_sel.rowMax()
                && ch >= m_sel.chMin() && ch <= m_sel.chMax();
            if (isSelected) {
                ImVec2 rMin = ImGui::GetCursorScreenPos();
                ImVec2 rMax = {rMin.x + m_cellW, rMin.y + m_rowH};
                ImGui::GetWindowDrawList()->AddRectFilled(
                    rMin, rMax, IM_COL32(80, 80, 200, 60));
            }

            float subX[5], subW[5];
            auto recordPos = [&](int col) { subX[col] = ImGui::GetCursorScreenPos().x; };

            std::string note  = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_note);
            std::string inst  = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_instrument);
            std::string volFx = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_volumeffect);
            std::string volV  = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_volume);
            std::string vol   = volFx + volV;
            std::string fx    = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_effect);
            std::string fxp   = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_parameter);

            bool hasNote = !note.empty() && note != "..." && note != "---";
            bool hasInst = !inst.empty() && inst != ".." && inst != "00";
            bool hasVol  = !volFx.empty() && volFx != "." && volFx != "..."
                        && !volV.empty()  && volV  != ".." && volV != ".";
            bool hasFx   = !fx.empty()    && fx    != ".";

            recordPos(0);
            ImGui::TextColored(hasNote ? kColNote : kDim, "%-3s", hasNote ? note.c_str() : "\xc2\xb7\xc2\xb7\xc2\xb7");
            subW[0] = ImGui::GetItemRectSize().x + 6;
            ImGui::SameLine(0, 6); recordPos(1);
            ImGui::TextColored(hasInst ? kColInstr : kDim, "%-2s", hasInst ? inst.c_str() : "\xc2\xb7\xc2\xb7");
            subW[1] = ImGui::GetItemRectSize().x + 6;
            ImGui::SameLine(0, 6); recordPos(2);
            ImGui::TextColored(hasVol ? kColVolume : kDim, "%-3s", hasVol ? vol.c_str() : "\xc2\xb7\xc2\xb7\xc2\xb7");
            subW[2] = ImGui::GetItemRectSize().x + 6;
            ImGui::SameLine(0, 6); recordPos(3);
            ImGui::TextColored(hasFx ? kColGlobal : kDim, "%s",   hasFx ? fx.c_str()  : "\xc2\xb7");
            subW[3] = ImGui::GetItemRectSize().x;
            ImGui::SameLine(0, 0); recordPos(4);
            ImGui::TextColored(hasFx ? kColGlobal : kDim, "%-2s", hasFx ? fxp.c_str() : "\xc2\xb7\xc2\xb7");
            subW[4] = ImGui::GetItemRectSize().x;

            if (isEditCell) {
                int col = int(m_cur.col);
                float y0 = ImGui::GetItemRectMin().y;
                float y1 = ImGui::GetItemRectMax().y;
                ImU32 hilite = IM_COL32(255, 220, 80, 80);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {subX[col]-1, y0}, {subX[col]+subW[col], y1+1}, hilite);
                float x0cell = subX[0] - 1;
                float x1cell = subX[4] + subW[4] + 1;
                ImGui::GetWindowDrawList()->AddRect(
                    {x0cell, y0-1}, {x1cell, y1+1}, IM_COL32(255,255,255,220));
            }
        }
    }
    }
    clipper.End();
    if (m_fontMono) ImGui::PopFont();
    ImGui::EndTable();
    ImGui::PopStyleColor(2);

}

// ── Cursor helpers ────────────────────────────────────────────────────────────

void TrackerView::clampCursor() {
    if (!m_mod) return;
    int32_t nrows = pe_num_rows(m_mod, m_state.current_pattern.load());
    int32_t nch   = m_state.num_channels.load();
    m_cur.row     = std::clamp(m_cur.row,     0, std::max(0, nrows-1));
    m_cur.channel = std::clamp(m_cur.channel, 0, std::max(0, nch-1));
}

void TrackerView::advanceCursor() {
    if (!m_mod || m_editStep == 0) return;
    int32_t nrows = pe_num_rows(m_mod, m_state.current_pattern.load());
    m_cur.row = std::min(m_cur.row + m_editStep, nrows - 1);
}

// ── Input processing (replaces Qt key event handlers) ────────────────────────

void TrackerView::processInput() {
    if (!m_mod) return;

    int32_t pat   = m_state.current_pattern.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    int32_t nch   = m_state.num_channels.load();
    bool    shift = ImGui::GetIO().KeyShift;
    bool    ctrl  = ImGui::GetIO().KeyCtrl;

    // ── Ctrl shortcuts (Undo/Redo/Copy/Paste/Cut) ─────────────────────────────
    if (ctrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (shift) doRedo(); else doUndo();
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y)) { doRedo(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_C)) { selCopy(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_X)) { selCopy(); selDelete(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_V)) { selPaste(); return; }
    }

    // ── Insert / delete row (keyboard only) ────────────────────────────────────
    if (ImGui::IsKeyPressed(ImGuiKey_Insert))    { insertRow(); return; }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace))  { deleteRow(); return; }

    // ── Navigation with optional shift-selection ──────────────────────────────
    auto navigate = [&](int32_t newRow, int32_t newCh, EditCol newCol) {
        if (shift) {
            if (!m_sel.active) {
                m_sel.active    = true;
                m_sel.rowAnchor = m_cur.row;
                m_sel.chAnchor  = m_cur.channel;
            }
            m_sel.rowCursor = newRow;
            m_sel.chCursor  = newCh;
        } else {
            m_sel.active = false;
        }
        m_cur.row     = newRow;
        m_cur.channel = newCh;
        m_cur.col     = newCol;
    };

    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        navigate(std::max(0, m_cur.row - 1), m_cur.channel, m_cur.col); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        navigate(std::min(nrows - 1, m_cur.row + 1), m_cur.channel, m_cur.col); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        int32_t newCh  = m_cur.channel;
        EditCol newCol = m_cur.col;
        if (newCol == EditCol::Note) {
            if (newCh > 0) { newCh--; newCol = EditCol::FxParam; }
        } else {
            newCol = EditCol(int(newCol) - 1);
        }
        navigate(m_cur.row, newCh, newCol); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        int32_t newCh  = m_cur.channel;
        EditCol newCol = m_cur.col;
        if (newCol == EditCol::FxParam) {
            if (newCh < nch - 1) { newCh++; newCol = EditCol::Note; }
        } else {
            newCol = EditCol(int(newCol) + 1);
        }
        navigate(m_cur.row, newCh, newCol); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        int32_t newCh = shift ? (m_cur.channel + nch - 1) % nch
                               : (m_cur.channel + 1) % nch;
        navigate(m_cur.row, newCh, EditCol::Note); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
        navigate(std::max(0, m_cur.row - 16), m_cur.channel, m_cur.col); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
        navigate(std::min(nrows - 1, m_cur.row + 16), m_cur.channel, m_cur.col); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        navigate(0, m_cur.channel, m_cur.col); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_End)) {
        navigate(nrows - 1, m_cur.channel, m_cur.col); return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F1))    { m_baseOctave = std::max(0, m_baseOctave - 1); return; }
    if (ImGui::IsKeyPressed(ImGuiKey_F2))    { m_baseOctave = std::min(8, m_baseOctave + 1); return; }
    if (ImGui::IsKeyPressed(ImGuiKey_Minus)) { m_editStep = std::max(0, m_editStep - 1); return; }
    if (ImGui::IsKeyPressed(ImGuiKey_Equal)) { m_editStep = std::min(16, m_editStep + 1); return; }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
        { m_editMode = !m_editMode; return; }

    if (!m_editMode) return;

    // Delete key: if selection active, delete selection; otherwise clear current cell
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (m_sel.active) {
            selDelete(); return;
        }
        // Clear current cell with undo
        PatternCell before;
        pe_get_cell(m_mod, pat, m_cur.row, m_cur.channel, before);
        pe_clear_cell(m_mod, pat, m_cur.row, m_cur.channel);
        PatternCell after{};
        UndoEntry e;
        e.push_back({pat, m_cur.row, m_cur.channel, before, after});
        pushEdit(std::move(e));
        advanceCursor(); return;
    }

    PatternCell cell;
    pe_get_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);

    if (m_cur.col == EditCol::Note) {
        if (ImGui::IsKeyPressed(ImGuiKey_1)) {
            PatternCell newCell = cell; newCell.note = NOTE_NOTECUT;
            setCell(pat, m_cur.row, m_cur.channel, newCell);
            advanceCursor(); return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
            PatternCell newCell = cell; newCell.note = NOTE_KEYOFF;
            setCell(pat, m_cur.row, m_cur.channel, newCell);
            advanceCursor(); return;
        }
        for (const auto& nm : kNoteKeys) {
            if (ImGui::IsKeyPressed(nm.key)) {
                int note = (m_baseOctave + nm.semi/12) * 12 + (nm.semi % 12) + 1;
                if (note >= NOTE_MIN && note <= NOTE_MAX) {
                    PatternCell newCell = cell; newCell.note = (uint8_t)note;
                    setCell(pat, m_cur.row, m_cur.channel, newCell);
                    advanceCursor();
                }
                return;
            }
        }
        return;
    }

    if (m_cur.col == EditCol::Instr) {
        for (const auto& hk : kHexKeys) {
            if (ImGui::IsKeyPressed(hk.key)) {
                PatternCell newCell = cell;
                newCell.instr = (uint8_t)(((cell.instr % 10) * 10 + hk.semi) % 100);
                setCell(pat, m_cur.row, m_cur.channel, newCell);
                advanceCursor(); return;
            }
        }
        return;
    }

    if (m_cur.col == EditCol::Vol) {
        for (int i = 0; i < 10; i++) {
            if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_0 + i))) {
                PatternCell newCell = cell;
                if (newCell.volcmd == VOLCMD_NONE) newCell.volcmd = VOLCMD_VOLUME;
                newCell.vol = (uint8_t)std::min((int)(cell.vol % 10) * 10 + i, 64);
                setCell(pat, m_cur.row, m_cur.channel, newCell);
                advanceCursor(); return;
            }
        }
        return;
    }

    if (m_cur.col == EditCol::FxCmd) {
        for (int i = 0; i < 26; i++) {
            if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_A + i))) {
                PatternCell newCell = cell; newCell.command = (uint8_t)(i + 1);
                setCell(pat, m_cur.row, m_cur.channel, newCell);
                advanceCursor(); return;
            }
        }
        return;
    }

    if (m_cur.col == EditCol::FxParam) {
        for (const auto& hk : kHexKeys) {
            if (ImGui::IsKeyPressed(hk.key)) {
                PatternCell newCell = cell;
                newCell.param = (uint8_t)(((cell.param & 0x0F) << 4) | hk.semi);
                setCell(pat, m_cur.row, m_cur.channel, newCell);
                advanceCursor(); return;
            }
        }
        return;
    }
}
