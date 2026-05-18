#pragma once
#include <atomic>
#include <array>
#include <string>
#include <mutex>
#include <vector>

static constexpr int MAX_CHANNELS = 64;

struct ChannelVU {
    float left  = 0.0f;
    float right = 0.0f;
};

// Shared state written by the audio thread, read by the UI thread.
// Numeric fields use atomics; string/vector fields use a mutex.
struct ModuleState {
    // Playback position
    std::atomic<int32_t> current_row     {0};
    std::atomic<int32_t> current_pattern {0};
    std::atomic<int32_t> current_order   {0};
    std::atomic<int32_t> current_speed   {6};
    std::atomic<double>  position_seconds{0.0};
    std::atomic<double>  duration_seconds{0.0};
    std::atomic<double>  bpm             {125.0};
    std::atomic<int32_t> num_channels    {0};
    std::atomic<int32_t> num_orders      {0};
    std::atomic<bool>    is_playing      {false};

    // Per-channel VU levels (written by audio thread under vu_mutex)
    std::mutex vu_mutex;
    std::array<ChannelVU, MAX_CHANNELS> vu{};

    // Module metadata (written once on load, read by UI)
    std::mutex meta_mutex;
    std::string title;
    std::string artist;
    std::string type_long;
    std::string tracker;
    int32_t     num_patterns  {0};
    int32_t     num_samples   {0};
    int32_t     num_instruments{0};
    std::vector<std::string> channel_names;
    std::vector<std::string> instrument_names;
    std::vector<std::string> sample_names;

    void reset() {
        current_row      = 0;
        current_pattern  = 0;
        current_order    = 0;
        current_speed    = 6;
        position_seconds = 0.0;
        duration_seconds = 0.0;
        bpm              = 125.0;
        num_channels     = 0;
        num_orders       = 0;
        is_playing       = false;
        {
            std::lock_guard<std::mutex> lk(vu_mutex);
            vu.fill({});
        }
        {
            std::lock_guard<std::mutex> lk(meta_mutex);
            title.clear(); artist.clear(); type_long.clear(); tracker.clear();
            num_patterns = num_samples = num_instruments = 0;
            channel_names.clear(); instrument_names.clear(); sample_names.clear();
        }
    }
};
