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

struct WaveSnapshot {
    float   samples[WAVE_BUF]{};
    int32_t count{0};
};

// Double-buffered VU + waveform snapshot — no mutex needed.
// Audio callback writes to vuBuf[vuWriteIdx] then flips the index.
// UI reads from vuBuf[1 - vuWriteIdx] (the buffer the callback is not touching).
struct VuSnapshot {
    std::array<ChannelVU,    MAX_CHANNELS> vu{};
    std::array<WaveSnapshot, MAX_CHANNELS> channelWave{};
    WaveSnapshot                           masterWave{};
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
    std::atomic<bool>    is_playing         {false};
    std::atomic<bool>    is_paused          {false};
    std::atomic<bool>    is_pattern_playing {false};

    // Master output VU (audio thread writes)
    std::atomic<float>    master_vu_left   {0.0f};
    std::atomic<float>    master_vu_right  {0.0f};

    // Channel mute mask — UI writes atomically, audio thread applies (bit N = ch N muted)
    std::atomic<uint64_t> channel_mute_mask{0};

    // Per-channel VU + waveform snapshots — double-buffered, lock-free
    VuSnapshot           vuBuf[2];
    std::atomic<int>     vuWriteIdx{0};

    // Global volume range for the loaded format (set on load, read by UI)
    std::atomic<int> gvol_max {128};

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

    static int gvolMaxForType(const std::string& type) {
        if (type == "mptm")                            return 256;
        if (type == "s3m" || type == "xm" || type == "mod") return 64;
        return 128; // it and everything else
    }

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
        is_playing         = false;
        is_paused          = false;
        is_pattern_playing = false;
        channel_mute_mask= 0;
        master_vu_left   = 0.0f;
        master_vu_right  = 0.0f;
        vuBuf[0] = {};
        vuBuf[1] = {};
        vuWriteIdx.store(0);
        gvol_max.store(128);
        {
            std::lock_guard<std::mutex> lk(meta_mutex);
            title.clear(); artist.clear(); type_long.clear(); tracker.clear();
            num_patterns = num_samples = num_instruments = 0;
            channel_names.clear(); instrument_names.clear();
            sample_names.clear(); pattern_names.clear();
        }
    }
};
