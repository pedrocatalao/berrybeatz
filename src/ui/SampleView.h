#pragma once
#include "OpenMPTInternal.h"
#include "ModuleController.h"
#include <cstdint>
#include <functional>
#include <vector>

struct ImFont;

class SampleView {
public:
    void setController(ModuleController* ctrl, int sampleCount);
    void clearModule();
    void setFonts(ImFont* mono, ImFont* ui);
    void setSelection(int idx);   // 1-based
    int  getSelection() const { return m_sel; }
    void setBaseOctave(int o)  { m_baseOctave = o; }
    void midiNoteOn(int midiNote, int vel);
    void midiNoteOff(int midiNote);
    void midiPitchBend(int raw14);  // 0–16383, center=8192
    void render(float x, float y, float w, float h);

    std::function<void()> onNeedAudio;

private:
    void loadSample();
    void saveSample();
    void drawEditor(float x, float y, float w, float h);
    void drawWaveform(float w, float h);
    void drawNoteKeyboard(float w, float h);

    ModuleController* m_ctrl{nullptr};
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
    int m_pitchBend{8192};   // 14-bit MIDI pitch bend, center=8192
    std::vector<float> m_waveMin, m_waveMax;
    int m_waveLen{0};
};
