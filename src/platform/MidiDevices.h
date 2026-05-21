#pragma once
#include <string>
#include <vector>

struct MidiSourceInfo {
    int         index;
    std::string name;
};

std::vector<MidiSourceInfo> enumerateMidiSources();
