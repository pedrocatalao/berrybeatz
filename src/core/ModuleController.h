#pragma once
#include "ModuleState.h"
#include "PatternEditor.h"
#include <cstdint>
#include <string>

namespace openmpt { class module_ext; }

// Single owner of all openmpt::module_ext mutations.
// UI code reads from module() but routes all writes through named commands here.
class ModuleController {
public:
    explicit ModuleController(ModuleState& state);

    void setModule(openmpt::module_ext* mod);
    void clearModule();
    openmpt::module_ext* module() const { return m_mod; }

    // Transport parameters
    void setTempo(int bpm);
    void setSpeed(int speed);
    void setGlobalVolume(double factor);  // 0.0–1.0
    void setTempoFactor(double factor);
    void setPitchFactor(double factor);

    // Render parameters
    void setMasterGain(int gain);
    void setStereoSeparation(int sep);
    void setInterpolation(int mode);
    void setVolumeRamping(int mode);

    // Pattern editing
    void setPatternCell(int32_t pat, int32_t row, int32_t ch, const PatternCell& cell);
    void clearPatternCell(int32_t pat, int32_t row, int32_t ch);
    void insertPatternRow(int32_t pat, int32_t row, int32_t nch);
    void deletePatternRow(int32_t pat, int32_t row, int32_t nch);
    int  insertPattern(int rows = 64);

    // Note preview (interactive extension — safe from UI thread)
    void     playNote(int32_t channel, int note, int instrument, double volume, double panning);
    void     stopNote(int32_t channel);
    void     setNotePitch(int32_t channel, double semitones);
    int32_t  playInstrumentNote(int32_t instrument, int note, double volume, double panning);
    int32_t  playSample(int sampleIndex, int note, double volume, double panning);

    // File
    bool save(const std::string& path, const std::string& type);

private:
    ModuleState&          m_state;
    openmpt::module_ext*  m_mod {nullptr};
};
