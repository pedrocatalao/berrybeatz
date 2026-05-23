#pragma once
#include "Settings.h"
#include "AudioEngine.h"
#include "MidiDevices.h"
#include <functional>
#include <vector>

class SettingsView {
public:
    std::function<void(const Settings&)> onApply;
    std::function<void(int)>             onMidiSourceChange;  // fires immediately on device select

    void open(const Settings& current,
              std::vector<AudioDeviceInfo> audioDevs,
              std::vector<MidiSourceInfo>  midiDevs);
    void draw();   // call every frame; modal is shown only when open
    bool isOpen()     const { return m_open; }
    bool isLearning() const;
    void feedMidiCC(int cc);  // call from App when a CC event arrives during learn

private:
    void drawAudioTab();
    void drawMidiTab();
    void drawEditorTab();
    void drawGeneralTab();

    bool     m_open        {false};
    bool     m_shouldOpen  {false};
    int      m_learnTarget {-1};   // -1=off, 0=oct,1=bpm,2=spd,3=rpb,4=vol
    Settings m_pending;
    std::vector<AudioDeviceInfo> m_audioDevs;
    std::vector<MidiSourceInfo>  m_midiDevs;
    int      m_audioDevSel {0};  // 0 = "(Default)"
    int      m_midiDevSel  {0};  // 0 = "(None)"
    float    m_winPosX     {-1.f};  // -1 = use center on next open
    float    m_winPosY     {-1.f};
};
