#pragma once
#include <string>
#include <vector>
#include <cstdint>

class SoundModule;

// Save module to path in its original format.
// 'type' is the string from SoundModule::getType(), e.g. "it", "xm", "s3m", "mod".
bool bb_save_module(SoundModule* mod, const std::string& path, const std::string& type);

// Insert a new empty pattern with the given row count.
// Returns the new pattern index, or -1 on failure.
int bb_insert_pattern(SoundModule* mod, int rows = 64);

// ─── Instrument data ─────────────────────────────────────────────────────────

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
    uint8_t  filterMode{0xFF};
    uint8_t  nna{0};
    uint8_t  dct{0};
    uint8_t  dna{0};
    uint8_t  resampling{5};
    uint8_t  noteMap[128]{};
    uint16_t keyboard[128]{};
    InstrumentEnvData volEnv, panEnv, pitchEnv;
    bool valid{false};
};

InstrumentData bb_get_instrument_data(SoundModule* mod, int instrIndex);
bool           bb_set_instrument_data(SoundModule* mod, int instrIndex, const InstrumentData& data);
int            bb_instrument_count(SoundModule* mod);
int            bb_sample_count(SoundModule* mod);

// ─── Sample data ─────────────────────────────────────────────────────────────

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
    uint8_t  loopType{0};
    uint8_t  sustainLoopType{0};
    bool     is16bit{false};
    bool     isStereo{false};
    bool     hasData{false};
    bool     isOPL{false};
    bool     valid{false};
};

SampleData bb_get_sample_data(SoundModule* mod, int sampleIndex);
bool       bb_set_sample_data(SoundModule* mod, int sampleIndex, const SampleData& data);
int32_t    bb_play_sample(SoundModule* mod, int sampleIndex, int note, double volume, double panning);
void       bb_build_sample_waveform(SoundModule* mod, int sampleIndex,
                                    std::vector<float>& waveMin, std::vector<float>& waveMax,
                                    int& outLen, int width = 1024);

// Disable FT2-style 5ms volume ramping.
void bb_set_ft2_volume_ramping(SoundModule* mod, bool enabled);

// Import raw 16-bit PCM into a sample slot. sampleIndex is 1-based.
// Replaces any existing sample data. Returns false on failure.
bool bb_import_sample_pcm(SoundModule* mod, int sampleIndex,
                           const int16_t* data, uint32_t frames,
                           int channels, uint32_t sampleRate,
                           const char* name = nullptr);

// Return 1-based index of the first sample slot with no PCM data, or -1 if none.
int  bb_find_free_sample(SoundModule* mod);

// Call BEFORE read() to arm per-channel capture.
void bb_setup_channel_capture(SoundModule* mod, int nChannels);

// Call AFTER read() to collect captured per-channel audio.
void bb_capture_channel_waves(SoundModule* mod,
                               float* samplesOut, int32_t* countsOut,
                               int nChannels, int perChannel);

// Call AFTER ratio normalisation to write OPL per-voice data.
void bb_apply_opl_channel_waves(SoundModule* mod,
                                 float* chanBuf, int nChannels, int perChannel);
