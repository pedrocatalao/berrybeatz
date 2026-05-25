#pragma once
#include <string>

struct MidiCCMap {
    int oct {-1};
    int bpm {-1};
    int spd {-1};
    int rpb {-1};
    int vol {-1};
};

struct Settings {
    int  audioDeviceIndex {-1};   // -1 = PortAudio default
    int  sampleRate       {48000};
    int  midiInputIndex   {-1};   // -1 = none
    bool followPlayback    {true};
    bool wrapCursor        {false};
    bool continuousScroll  {false};
    int  glowLogo    {5};   // 0 = off, 1–10 intensity
    int  glowVU      {5};
    int  glowPattern {5};
    int  glowKnobs   {5};
    MidiCCMap midiCC;

    void load(const std::string& path);
    void save(const std::string& path) const;
    static std::string defaultPath();
};
