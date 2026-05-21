#pragma once
#include "OpenMPTInternal.h"
#include <cstdint>
#include <functional>
#include <vector>

namespace openmpt { class module_ext; }
struct ImFont;

class SampleView {
public:
    void setModule(openmpt::module_ext* mod, int sampleCount);
    void clearModule();
    void setFonts(ImFont* mono, ImFont* ui);
    void setSelection(int idx);   // 1-based
    int  getSelection() const { return m_sel; }
    void setBaseOctave(int o)  { m_baseOctave = o; }
    void midiNoteOn(int midiNote, int vel);
    void midiNoteOff(int midiNote);
    void render(float x, float y, float w, float h);

    std::function<void()> onNeedAudio;

private:
    void loadSample();
    void saveSample();
    void drawEditor(float x, float y, float w, float h);
    void drawWaveform(float w, float h);
    void drawNoteKeyboard(float w, float h);

    openmpt::module_ext* m_mod{nullptr};
    ImFont* m_fontMono{nullptr};
    ImFont* m_fontUI{nullptr};
    int m_sampleCount{0};
    int m_sel{1};
    SampleData m_data;
    bool m_dirty{false};
    int32_t m_playChannel{-1};
    int m_playNote{-1};
    int m_baseOctave{4};
    int m_kbChannels[128];   // per-note play channel; -1 = not playing
    std::vector<float> m_waveMin, m_waveMax;
    int m_waveLen{0};
};
