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

// ─── Sample data POD struct ───────────────────────────────────────────────────

struct SampleData {
    char     name[32]{};
    char     filename[32]{};
    uint32_t length{0};
    uint32_t loopStart{0};
    uint32_t loopEnd{0};
    uint32_t sustainStart{0};
    uint32_t sustainEnd{0};
    uint32_t c5speed{8363};
    uint16_t volume{256};
    uint16_t globalVol{64};
    uint16_t pan{128};
    bool     panEnabled{false};
    int8_t   relativeTone{0};
    int8_t   fineTune{0};
    uint8_t  vibType{0};
    uint8_t  vibSweep{0};
    uint8_t  vibDepth{0};
    uint8_t  vibRate{0};
    uint8_t  loopType{0};        // 0=none, 1=forward, 2=pingpong
    uint8_t  sustainLoopType{0}; // 0=none, 1=forward, 2=pingpong
    bool     is16bit{false};
    bool     isStereo{false};
    bool     hasData{false};
    bool     valid{false};
};

// sampleIndex is 1-based
SampleData bb_get_sample_data(openmpt::module_ext* mod, int sampleIndex);
bool       bb_set_sample_data(openmpt::module_ext* mod, int sampleIndex, const SampleData& data);
// note is OpenMPT 1-based (1=C-0, 61=C-5=middle C). Returns channel index or -1.
int32_t    bb_play_sample(openmpt::module_ext* mod, int sampleIndex, int note, double volume, double panning);
// Fills waveMin[width] and waveMax[width] with normalized [-1,1] sample data. outLen set to width on success.
void       bb_build_sample_waveform(openmpt::module_ext* mod, int sampleIndex, std::vector<float>& waveMin, std::vector<float>& waveMax, int& outLen, int width = 1024);

// Disable FT2-style 5ms volume ramping (kFT2VolumeRamping playBehaviour).
// libopenmpt enables this by default for genuine FT2 XM files; OpenMPT desktop does not.
void bb_disable_ft2_volume_ramping(openmpt::module_ext* mod);

// Call BEFORE read() to arm per-channel capture.
void bb_setup_channel_capture(openmpt::module_ext* mod, int nChannels);

// Call AFTER read() to collect captured per-channel audio.
// samplesOut: flat [nChannels * perChannel] floats (left channel, amplitude-correct).
void bb_capture_channel_waves(openmpt::module_ext* mod,
                               float* samplesOut, int32_t* countsOut,
                               int nChannels, int perChannel);
