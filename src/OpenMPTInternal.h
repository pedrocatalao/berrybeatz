#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace openmpt { class module_ext; }

// Save module to path in its original format.
// 'type' is the string returned by module->get_metadata("type"), e.g. "it", "xm", "s3m", "mod".
// Returns true on success.
bool bb_save_module(openmpt::module_ext* mod, const std::string& path, const std::string& type);

// Insert a new empty pattern with the given row count at the lowest available index.
// Returns the new pattern index, or -1 on failure.
int bb_insert_pattern(openmpt::module_ext* mod, int rows = 64);

// ─── Instrument data POD structs (no soundlib includes needed) ────────────────

struct InstrumentEnvData {
    struct Node { uint16_t tick; uint8_t value; };
    std::vector<Node> nodes;
    bool enabled{false}, loop{false}, sustain{false}, carry{false}, filter{false};
    uint8_t loopStart{0}, loopEnd{0}, sustainStart{0}, sustainEnd{0};
};

struct InstrumentData {
    char     name[32]{};
    char     filename[32]{};
    uint16_t globalVol{64};
    uint16_t fadeOut{256};
    uint16_t pan{128};
    bool     panEnabled{false};
    uint8_t  volSwing{0};
    uint8_t  panSwing{0};
    int8_t   pps{0};
    uint8_t  ppc{48};
    uint8_t  cutoff{0};
    bool     cutoffEnabled{false};
    uint8_t  resonance{0};
    bool     resonanceEnabled{false};
    uint8_t  cutSwing{0};
    uint8_t  resSwing{0};
    uint8_t  filterMode{0xFF};  // 0=LowPass, 1=HighPass, 0xFF=Unchanged
    uint8_t  nna{0};            // NewNoteAction cast to uint8
    uint8_t  dct{0};            // DuplicateCheckType cast to uint8
    uint8_t  dna{0};            // DuplicateNoteAction cast to uint8
    uint8_t  resampling{5};     // ResamplingMode (5=SRCMODE_DEFAULT)
    uint8_t  noteMap[128]{};    // note remapping
    uint16_t keyboard[128]{};   // sample index per note
    InstrumentEnvData volEnv, panEnv, pitchEnv;
    bool valid{false};
};

// instrIndex is 1-based
InstrumentData bb_get_instrument_data(openmpt::module_ext* mod, int instrIndex);
bool           bb_set_instrument_data(openmpt::module_ext* mod, int instrIndex, const InstrumentData& data);
int            bb_instrument_count(openmpt::module_ext* mod);
int            bb_sample_count(openmpt::module_ext* mod);
