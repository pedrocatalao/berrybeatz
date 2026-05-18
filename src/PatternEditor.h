#pragma once
#include <cstdint>

namespace openmpt { class module_ext; }

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

int  pe_num_rows(openmpt::module_ext* mod, int pat);
void pe_get_cell(openmpt::module_ext* mod, int pat, int row, int ch, PatternCell& out);
void pe_set_cell(openmpt::module_ext* mod, int pat, int row, int ch, const PatternCell& cell);
void pe_clear_cell(openmpt::module_ext* mod, int pat, int row, int ch);
