#pragma once
#include "OpenMPTInternal.h"
#include "ModuleController.h"
#include <cstdint>
#include <functional>

struct ImFont;

class InstrumentView {
public:
    void setController(ModuleController* ctrl, int instrCount);
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
    std::function<void(int instrIdx, int opnote, float vol, float pan, int key)> onEnqueuePlay;
    std::function<void(int key)>          onEnqueueStop;
    std::function<void(float semitones)>  onEnqueuePitch;

private:
    void loadInstrument();
    void saveInstrument();
    void drawEditor(float x, float y, float w, float h);
    void drawEnvelopeGraph(InstrumentEnvData& env, const char* typeLabel, float w, float h);
    void drawNoteKeyboard(float w, float h);

    ModuleController* m_ctrl{nullptr};
    ImFont* m_fontMono{nullptr};
    ImFont* m_fontUI{nullptr};
    int m_instrCount{0};
    int m_sel{1};
    InstrumentData m_data;
    bool m_dirty{false};
    int m_activeEnv{0};       // 0=vol 1=pan 2=pitch
    int m_dragNode{-1};
    bool    m_playHeld{false};
    int     m_playNote{-1};
    int     m_baseOctave{4};
    bool    m_kbHeld[128]{};
    int     m_pitchBend{8192};
};
