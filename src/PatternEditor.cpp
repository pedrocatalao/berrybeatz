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
