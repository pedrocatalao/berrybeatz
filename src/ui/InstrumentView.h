#pragma once
#include "OpenMPTInternal.h"
#include <cstdint>
#include <functional>

namespace openmpt { class module_ext; }
struct ImFont;

class InstrumentView {
public:
    void setModule(openmpt::module_ext* mod, int instrCount);
    void clearModule();
    void setFonts(ImFont* mono, ImFont* ui);
    void setSelection(int idx);   // 1-based; clamps to valid range
    int  getSelection() const { return m_sel; }
    void setBaseOctave(int o)  { m_baseOctave = o; }
    void midiNoteOn(int midiNote, int vel);
    void midiNoteOff(int midiNote);
    void midiPitchBend(int raw14);  // 0–16383, center=8192
    void render(float x, float y, float w, float h);

    // Called when a note preview starts — wire this to ensure the audio engine is running.
    std::function<void()> onNeedAudio;

private:
    void loadInstrument();
    void saveInstrument();
    void drawEditor(float x, float y, float w, float h);
    void drawEnvelopeGraph(InstrumentEnvData& env, const char* typeLabel, float w, float h);
    void drawNoteKeyboard(float w, float h);

    openmpt::module_ext* m_mod{nullptr};
    ImFont* m_fontMono{nullptr};
    ImFont* m_fontUI{nullptr};
    int m_instrCount{0};
    int m_sel{1};
    InstrumentData m_data;
    bool m_dirty{false};
    int m_activeEnv{0};       // 0=vol 1=pan 2=pitch
    int m_dragNode{-1};
    int32_t m_playChannel{-1};
    int m_playNote{-1};
    int m_baseOctave{4};
    int m_kbChannels[128];   // per-note play channel; -1 = not playing
};
