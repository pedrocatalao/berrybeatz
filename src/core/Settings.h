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
    bool followPlayback   {true};
    bool wrapCursor       {false};
    int  glowLevel        {5};     // 0 = off, 5 = default (1.0×), 10 = max (2.0×)
    bool glowLogo         {true};
    bool glowVU           {true};
    bool glowPattern      {true};
    bool glowKnobs        {true};
    MidiCCMap midiCC;

    void load(const std::string& path);
    void save(const std::string& path) const;
    static std::string defaultPath();
};
