#include "PatternEditor.h"
#include "SoundModule.h"

int pe_num_rows(SoundModule* mod, int pat) {
    return mod->getPatternNumRows(pat);
}

void pe_get_cell(SoundModule* mod, int pat, int row, int ch, PatternCell& out) {
    mod->getPatternCell(pat, row, ch, out);
}

void pe_set_cell(SoundModule* mod, int pat, int row, int ch, const PatternCell& cell) {
    mod->setPatternCell(pat, row, ch, cell);
}

void pe_clear_cell(SoundModule* mod, int pat, int row, int ch) {
    mod->clearPatternCell(pat, row, ch);
}

void pe_insert_row(SoundModule* mod, int pat, int row, int nch) {
    int nrows = mod->getPatternNumRows(pat);
    if (nrows <= 0) return;
    for (int r = nrows - 2; r >= row; r--) {
        for (int ch = 0; ch < nch; ch++) {
            PatternCell cell;
            mod->getPatternCell(pat, r, ch, cell);
            mod->setPatternCell(pat, r + 1, ch, cell);
        }
    }
    for (int ch = 0; ch < nch; ch++)
        mod->clearPatternCell(pat, row, ch);
}

void pe_delete_row(SoundModule* mod, int pat, int row, int nch) {
    int nrows = mod->getPatternNumRows(pat);
    if (nrows <= 0) return;
    for (int r = row + 1; r < nrows; r++) {
        for (int ch = 0; ch < nch; ch++) {
            PatternCell cell;
            mod->getPatternCell(pat, r, ch, cell);
            mod->setPatternCell(pat, r - 1, ch, cell);
        }
    }
    for (int ch = 0; ch < nch; ch++)
        mod->clearPatternCell(pat, nrows - 1, ch);
}
