#include "PatternEditor.h"
#include <libopenmpt/libopenmpt_ext.hpp>

int pe_num_rows(openmpt::module_ext* mod, int pat) {
    return mod->GetPatternNumRows(pat);
}

void pe_get_cell(openmpt::module_ext* mod, int pat, int row, int ch, PatternCell& out) {
    mod->GetPatternCell(pat, row, ch,
        &out.note, &out.instr, &out.volcmd, &out.vol, &out.command, &out.param);
}

void pe_set_cell(openmpt::module_ext* mod, int pat, int row, int ch, const PatternCell& cell) {
    mod->SetPatternCell(pat, row, ch,
        cell.note, cell.instr, cell.volcmd, cell.vol, cell.command, cell.param);
}

void pe_clear_cell(openmpt::module_ext* mod, int pat, int row, int ch) {
    mod->ClearPatternCell(pat, row, ch);
}

void pe_insert_row(openmpt::module_ext* mod, int pat, int row, int nch) {
    int nrows = mod->GetPatternNumRows(pat);
    if (nrows <= 0) return;
    // Shift rows downward: start from last-1, copy to last
    for (int r = nrows - 2; r >= row; r--) {
        for (int ch = 0; ch < nch; ch++) {
            PatternCell cell;
            mod->GetPatternCell(pat, r, ch,
                &cell.note, &cell.instr, &cell.volcmd, &cell.vol, &cell.command, &cell.param);
            mod->SetPatternCell(pat, r + 1, ch,
                cell.note, cell.instr, cell.volcmd, cell.vol, cell.command, cell.param);
        }
    }
    // Clear the inserted row
    for (int ch = 0; ch < nch; ch++)
        mod->ClearPatternCell(pat, row, ch);
}

void pe_delete_row(openmpt::module_ext* mod, int pat, int row, int nch) {
    int nrows = mod->GetPatternNumRows(pat);
    if (nrows <= 0) return;
    // Shift rows upward: copy row+1..nrows-1 up by one
    for (int r = row + 1; r < nrows; r++) {
        for (int ch = 0; ch < nch; ch++) {
            PatternCell cell;
            mod->GetPatternCell(pat, r, ch,
                &cell.note, &cell.instr, &cell.volcmd, &cell.vol, &cell.command, &cell.param);
            mod->SetPatternCell(pat, r - 1, ch,
                cell.note, cell.instr, cell.volcmd, cell.vol, cell.command, cell.param);
        }
    }
    // Clear the last row
    for (int ch = 0; ch < nch; ch++)
        mod->ClearPatternCell(pat, nrows - 1, ch);
}
