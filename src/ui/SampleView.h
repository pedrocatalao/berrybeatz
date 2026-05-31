#pragma once
#include "SoundFileOps.h"
#include "ModuleController.h"
#include "PianoKeyboard.h"
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
    void setBaseOctave(int o)  { m_kbState.baseOctave = o; }
    void midiNoteOn(int midiNote, int vel);
    void midiNoteOff(int midiNote);
    void midiPitchBend(int raw14);  // 0–16383, center=8192
    void render(float x, float y, float w, float h);

    std::function<void()> onNeedAudio;

    // Called from CoreMIDI RT thread to update display state without going through the view's MIDI path.
    void setKbHeld(int note, bool held) { if (note >= 0 && note < 128) m_kbState.kbHeld[note] = held; }

    // Preview note routing — called instead of bb_play_sample/stopNote directly.
    // key: 0-127 = MIDI/keyboard slot, 128 = click-play slot.
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
    int m_sel{1};
    SampleData m_data;
    bool m_dirty{false};
    bool               m_showKeyboard{true};
    PianoKeyboardState m_kbState;
    int     m_pitchBend{8192};   // 14-bit MIDI pitch bend, center=8192
    std::vector<float> m_waveMin, m_waveMax;
    int m_waveLen{0};
};
