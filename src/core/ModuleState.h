#pragma once
#include <atomic>
#include <array>
#include <string>
#include <mutex>
#include <vector>

static constexpr int MAX_CHANNELS = 64;
static constexpr int WAVE_BUF     = 512;  // one full audio block worth of samples

struct ChannelVU {
    float left  = 0.0f;
    float right = 0.0f;
};

// Snapshot of actual signed audio samples from the last rendered block.
// Used for oscilloscope display. Protected by vu_mutex.
struct WaveSnapshot {
    float   samples[WAVE_BUF]{};
    int32_t count{0};
};

struct ModuleState {
    // Playback position (audio thread writes, UI reads)
    std::atomic<int32_t> current_row      {0};
    std::atomic<int32_t> current_pattern  {0};
    std::atomic<int32_t> current_order    {0};
    std::atomic<int32_t> current_speed    {6};
    std::atomic<int32_t> rows_per_beat    {4};
    std::atomic<double>  position_seconds {0.0};
    std::atomic<double>  duration_seconds {0.0};
    std::atomic<double>  bpm              {125.0};
    std::atomic<int32_t> num_channels     {0};
    std::atomic<int32_t> num_orders       {0};
    std::atomic<bool>    is_playing       {false};

    // Master output VU (audio thread writes)
    std::atomic<float>    master_vu_left   {0.0f};
    std::atomic<float>    master_vu_right  {0.0f};

    // Channel mute mask — UI writes atomically, audio thread applies (bit N = ch N muted)
    std::atomic<uint64_t> channel_mute_mask{0};

    // Per-channel VU + oscilloscope snapshots (audio thread writes under vu_mutex)
    std::mutex vu_mutex;
    std::array<ChannelVU,    MAX_CHANNELS> vu{};
    std::array<WaveSnapshot, MAX_CHANNELS> channelWave{};
    WaveSnapshot masterWave{};

    // Module metadata (written once on load)
    std::mutex meta_mutex;
    std::string title;
    std::string artist;
    std::string type_long;
    std::string tracker;
    int32_t     num_patterns   {0};
    int32_t     num_samples    {0};
    int32_t     num_instruments{0};
    std::vector<std::string> channel_names;
    std::vector<std::string> instrument_names;
    std::vector<std::string> sample_names;
    std::vector<std::string> pattern_names;

    void reset() {
        current_row      = 0;
        current_pattern  = 0;
        current_order    = 0;
        current_speed    = 6;
        rows_per_beat    = 4;
        position_seconds = 0.0;
        duration_seconds = 0.0;
        bpm              = 125.0;
        num_channels     = 0;
        num_orders       = 0;
        is_playing       = false;
        channel_mute_mask= 0;
        master_vu_left   = 0.0f;
        master_vu_right  = 0.0f;
        {
            std::lock_guard<std::mutex> lk(vu_mutex);
            vu.fill({});
            channelWave.fill({});
            masterWave = {};
        }
        {
            std::lock_guard<std::mutex> lk(meta_mutex);
            title.clear(); artist.clear(); type_long.clear(); tracker.clear();
            num_patterns = num_samples = num_instruments = 0;
            channel_names.clear(); instrument_names.clear();
            sample_names.clear(); pattern_names.clear();
        }
    }
};
