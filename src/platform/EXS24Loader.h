#pragma once
#include <cstdint>
#include <string>
#include <vector>

// One mapped zone from an EXS24 instrument.
struct EXS24Zone {
    uint8_t  noteLow   {0};
    uint8_t  noteHigh  {127};
    uint8_t  rootNote  {60};
    uint8_t  velLow    {0};
    uint8_t  velHigh   {127};
    int8_t   coarseTune{0};
    int8_t   fineTune  {0};
    std::string samplePath;   // absolute path to the audio file
    std::string sampleName;   // display name (no path, no extension)
};

struct EXS24Instrument {
    std::string name;
    std::vector<EXS24Zone> zones;
};

// Load and parse an .exs file.  Returns true on success.
bool loadEXS24(const std::string& path, EXS24Instrument& out);
