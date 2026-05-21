#pragma once
#include "Settings.h"
#include "AudioEngine.h"
#include "MidiDevices.h"
#include <functional>
#include <vector>

class SettingsView {
public:
    std::function<void(const Settings&)> onApply;

    void open(const Settings& current,
              std::vector<AudioDeviceInfo> audioDevs,
              std::vector<MidiSourceInfo>  midiDevs);
    void draw();   // call every frame; modal is shown only when open
    bool isOpen() const { return m_open; }

private:
    void drawAudioTab();
    void drawMidiTab();
    void drawEditorTab();

    bool     m_open       {false};
    bool     m_shouldOpen {false};
    Settings m_pending;
    std::vector<AudioDeviceInfo> m_audioDevs;
    std::vector<MidiSourceInfo>  m_midiDevs;
    int      m_audioDevSel {0};  // 0 = "(Default)"
    int      m_midiDevSel  {0};  // 0 = "(None)"
};
