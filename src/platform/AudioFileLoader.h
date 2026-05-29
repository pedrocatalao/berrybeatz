#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct DecodedAudio {
    std::vector<int16_t> pcm;   // interleaved
    uint32_t sampleRate {0};
    int      channels   {1};    // 1 or 2
};

// Decode any format macOS supports (CAF, AIFF, WAV, AAC, ALAC…) to 16-bit PCM.
// Returns false on failure.
bool loadAudioFile(const std::string& path, DecodedAudio& out);
