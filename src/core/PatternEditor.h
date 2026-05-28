#pragma once
#include <cstdint>

class SoundModule;

// Constants mirroring OpenMPT values so callers don't need soundlib headers
static constexpr uint8_t NOTE_NONE    = 0;
static constexpr uint8_t NOTE_MIN     = 1;
static constexpr uint8_t NOTE_MAX     = 128;
static constexpr uint8_t NOTE_KEYOFF  = 0xFF;
static constexpr uint8_t NOTE_NOTECUT = 0xFE;

// Volume commands (subset)
static constexpr uint8_t VOLCMD_NONE   = 0;
static constexpr uint8_t VOLCMD_VOLUME = 1;

// Effect commands
static constexpr uint8_t CMD_NONE = 0;

struct PatternCell {
    uint8_t note    {NOTE_NONE};
    uint8_t instr   {0};
    uint8_t volcmd  {VOLCMD_NONE};
    uint8_t vol     {0};
    uint8_t command {CMD_NONE};
    uint8_t param   {0};
};

int  pe_num_rows(SoundModule* mod, int pat);
void pe_get_cell(SoundModule* mod, int pat, int row, int ch, PatternCell& out);
void pe_set_cell(SoundModule* mod, int pat, int row, int ch, const PatternCell& cell);
void pe_clear_cell(SoundModule* mod, int pat, int row, int ch);

// Shift all rows from 'row' down by one; the last row is lost. The inserted row is cleared.
void pe_insert_row(SoundModule* mod, int pat, int row, int nch);
// Shift all rows from 'row+1' up by one; the last row is cleared.
void pe_delete_row(SoundModule* mod, int pat, int row, int nch);
