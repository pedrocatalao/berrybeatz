#include "TrackerView.h"
#include "PatternEditor.h"
#include <libopenmpt/libopenmpt_ext.hpp>
#include <imgui.h>
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

// ── Single-cell edit with undo tracking ──────────────────────────────────────

void TrackerView::setCell(int32_t pat, int32_t row, int32_t ch, const PatternCell& cell) {
    PatternCell before;
    pe_get_cell(m_mod, pat, row, ch, before);
    pe_set_cell(m_mod, pat, row, ch, cell);
    UndoEntry e;
    e.push_back({pat, row, ch, before, cell});
    m_undo.push(std::move(e));
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
    if (!e.empty()) m_undo.push(std::move(e));
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
    if (!e.empty()) m_undo.push(std::move(e));
}

void TrackerView::insertRow() {
    if (!m_mod) return;
    int32_t pat   = m_state.current_pattern.load();
    int32_t nch   = m_state.num_channels.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    UndoEntry e;
    e.reserve((nrows - m_cur.row) * nch);
    for (int32_t r = m_cur.row; r < nrows; r++) {
        for (int32_t c = 0; c < nch; c++) {
            PatternCell before;
            pe_get_cell(m_mod, pat, r, c, before);
            e.push_back({pat, r, c, before, PatternCell{}});
        }
    }
    pe_insert_row(m_mod, pat, m_cur.row, nch);
    for (auto& ce : e)
        pe_get_cell(m_mod, pat, ce.row, ce.ch, ce.after);
    m_undo.push(std::move(e));
}

void TrackerView::deleteRow() {
    if (!m_mod) return;
    int32_t pat   = m_state.current_pattern.load();
    int32_t nch   = m_state.num_channels.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    UndoEntry e;
    e.reserve((nrows - m_cur.row) * nch);
    for (int32_t r = m_cur.row; r < nrows; r++) {
        for (int32_t c = 0; c < nch; c++) {
            PatternCell before;
            pe_get_cell(m_mod, pat, r, c, before);
            e.push_back({pat, r, c, before, PatternCell{}});
        }
    }
    pe_delete_row(m_mod, pat, m_cur.row, nch);
    for (auto& ce : e)
        pe_get_cell(m_mod, pat, ce.row, ce.ch, ce.after);
    m_undo.push(std::move(e));
}

// ── Input processing ──────────────────────────────────────────────────────────

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
            if (shift) m_undo.redo(m_mod); else m_undo.undo(m_mod);
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y)) { m_undo.redo(m_mod); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_C)) { selCopy(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_X)) { selCopy(); selDelete(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_V)) { selPaste(); return; }
    }

    // ── Insert / delete row ───────────────────────────────────────────────────
    if (ImGui::IsKeyPressed(ImGuiKey_Insert))   { insertRow(); return; }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) { deleteRow(); return; }

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

    // Delete key: selection → delete selection; otherwise clear current cell
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (m_sel.active) { selDelete(); return; }
        PatternCell before;
        pe_get_cell(m_mod, pat, m_cur.row, m_cur.channel, before);
        pe_clear_cell(m_mod, pat, m_cur.row, m_cur.channel);
        PatternCell after{};
        UndoEntry e;
        e.push_back({pat, m_cur.row, m_cur.channel, before, after});
        m_undo.push(std::move(e));
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
