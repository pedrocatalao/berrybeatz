#pragma once
#include <string>

struct Settings {
    int  audioDeviceIndex {-1};   // -1 = PortAudio default
    int  sampleRate       {48000};
    int  midiInputIndex   {-1};   // -1 = none
    bool followPlayback   {true};
    bool wrapCursor       {false};

    void load(const std::string& path);
    void save(const std::string& path) const;
    static std::string defaultPath();
};
