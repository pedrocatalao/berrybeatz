#include "Settings.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>

std::string Settings::defaultPath()
{
    const char* home = std::getenv("HOME");
    std::string dir = home ? std::string(home) + "/Library/Application Support/BerryBeatz" : ".";
    std::filesystem::create_directories(dir);
    return dir + "/settings.cfg";
}

void Settings::load(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        try {
            if      (key == "audioDevice")    audioDeviceIndex = std::stoi(val);
            else if (key == "sampleRate")     sampleRate       = std::stoi(val);
            else if (key == "midiInput")      midiInputIndex   = std::stoi(val);
            else if (key == "followPlayback") followPlayback   = (val == "1");
            else if (key == "wrapCursor")       wrapCursor       = (val == "1");
            else if (key == "continuousScroll") continuousScroll = (val == "1");
            else if (key == "glowLogo")    glowLogo    = std::stoi(val);
            else if (key == "glowVU")      glowVU      = std::stoi(val);
            else if (key == "glowPattern") glowPattern = std::stoi(val);
            else if (key == "glowKnobs")   glowKnobs   = std::stoi(val);
            else if (key == "ccOct")          midiCC.oct       = std::stoi(val);
            else if (key == "ccBpm")          midiCC.bpm       = std::stoi(val);
            else if (key == "ccSpd")          midiCC.spd       = std::stoi(val);
            else if (key == "ccRpb")          midiCC.rpb       = std::stoi(val);
            else if (key == "ccVol")          midiCC.vol       = std::stoi(val);
        } catch (const std::exception&) {}
    }
}

void Settings::save(const std::string& path) const
{
    std::ofstream f(path);
    if (!f) return;
    f << "audioDevice="    << audioDeviceIndex         << "\n";
    f << "sampleRate="     << sampleRate               << "\n";
    f << "midiInput="      << midiInputIndex           << "\n";
    f << "followPlayback=" << (followPlayback ? "1" : "0") << "\n";
    f << "wrapCursor="       << (wrapCursor       ? "1" : "0") << "\n";
    f << "continuousScroll=" << (continuousScroll ? "1" : "0") << "\n";
    f << "glowLogo="    << glowLogo    << "\n";
    f << "glowVU="      << glowVU      << "\n";
    f << "glowPattern=" << glowPattern << "\n";
    f << "glowKnobs="   << glowKnobs   << "\n";
    f << "ccOct="          << midiCC.oct << "\n";
    f << "ccBpm="          << midiCC.bpm << "\n";
    f << "ccSpd="          << midiCC.spd << "\n";
    f << "ccRpb="          << midiCC.rpb << "\n";
    f << "ccVol="          << midiCC.vol << "\n";
}
