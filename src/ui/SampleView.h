#pragma once
#include "SoundFileOps.h"
#include "ModuleController.h"
#include <atomic>
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
    void midiNoteOn(int midiNote, int vel);
    void midiNoteOff(int midiNote);
    void midiPitchBend(int raw14);  // 0–16383, center=8192
    void render(float x, float y, float w, float h);

    std::function<void()> onNeedAudio;

    std::function<void(int sampleIdx, int opnote, float vol, float pan, int key)> onEnqueuePlay;
    std::function<void(int key)>          onEnqueueStop;
    std::function<void(float semitones)>  onEnqueuePitch;

private:
    void loadSample();
    void saveSample();
    void drawEditor(float x, float y, float w, float h);
    void drawWaveform(float w, float h);

    ModuleController* m_ctrl{nullptr};
    ImFont* m_fontMono{nullptr};
    ImFont* m_fontUI{nullptr};
    int m_sampleCount{0};
    std::atomic<int> m_sel{1};
    SampleData m_data;
    bool m_dirty{false};
    int  m_pitchBend{8192};
    std::vector<float> m_waveMin, m_waveMax;
    int m_waveLen{0};
};
