#include "AudioEngine.h"
#include <libopenmpt/libopenmpt_ext.hpp>
#include <portaudio.h>
#include <fstream>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <thread>
#include <chrono>

static constexpr int SAMPLE_RATE  = 48000;
static constexpr int FRAMES_BLOCK = 480; // 10ms per block

struct PaCallbackData {
    openmpt::module_ext*  mod;
    ModuleState*          state;
    std::atomic<bool>*    stop;
    std::atomic<bool>*    paused;
    std::atomic<double>*  seek;
    std::atomic<int32_t>* seek_order;
    std::atomic<int32_t>* subsong;
    std::vector<float>    left;
    std::vector<float>    right;
    uint64_t              appliedMuteMask{0};

    // Pattern play trigger pointers (owned by AudioEngine)
    std::atomic<int32_t>* pat_play_order    {nullptr};
    std::atomic<int32_t>* pat_play_startrow {nullptr};
    std::atomic<bool>*    pat_play_loop     {nullptr};
    std::atomic<bool>*    pat_play_single   {nullptr};

    // Pattern play in-progress state (callback-thread only)
    bool    inPatternPlay   {false};
    int32_t patPlayOrder    {-1};
    int32_t patPlayStartRow {0};
    bool    patPlayLoop     {false};
    bool    patPlaySingle   {false};
    int32_t patPrevRow      {-1};

    PaCallbackData()
        : mod(nullptr), state(nullptr), stop(nullptr), paused(nullptr),
          seek(nullptr), seek_order(nullptr), subsong(nullptr) {
        left.resize(FRAMES_BLOCK);
        right.resize(FRAMES_BLOCK);
    }
};

AudioEngine::AudioEngine(ModuleState& state)
    : m_state(state) {}

AudioEngine::~AudioEngine() {
    stop();
    join();
}

bool AudioEngine::loadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (onLoadFailed) onLoadFailed("Cannot open file: " + path);
        return false;
    }
    try {
        m_mod = std::make_unique<openmpt::module_ext>(f);
        fillState();
        return true;
    } catch (const std::exception& e) {
        if (onLoadFailed) onLoadFailed(std::string("Failed to load module: ") + e.what());
        return false;
    }
}

void AudioEngine::fillState() {
    if (!m_mod) return;
    std::lock_guard<std::mutex> lk(m_state.meta_mutex);
    m_state.title             = m_mod->get_metadata("title");
    m_state.artist            = m_mod->get_metadata("artist");
    m_state.type_long         = m_mod->get_metadata("type_long");
    m_state.tracker           = m_mod->get_metadata("tracker");
    m_state.num_patterns      = m_mod->get_num_patterns();
    m_state.num_samples       = m_mod->get_num_samples();
    m_state.num_instruments   = m_mod->get_num_instruments();
    m_state.channel_names     = m_mod->get_channel_names();
    m_state.instrument_names  = m_mod->get_instrument_names();
    m_state.sample_names      = m_mod->get_sample_names();
    m_state.pattern_names     = m_mod->get_pattern_names();
    m_state.num_channels      = m_mod->get_num_channels();
    m_state.num_orders        = m_mod->get_num_orders();
    m_state.duration_seconds  = m_mod->get_duration_seconds();
}

void AudioEngine::start() {
    m_stop.store(false);
    m_paused.store(false);
    m_thread = std::thread(&AudioEngine::run, this);
}

void AudioEngine::stop()  { m_stop.store(true); }
void AudioEngine::join()  { if (m_thread.joinable()) m_thread.join(); }
bool AudioEngine::isRunning() const { return m_running.load(); }

void AudioEngine::rewind() {
    m_stop.store(true);
    join();
    if (m_mod) m_mod->set_position_seconds(0.0);
    m_stop.store(false);
    m_paused.store(false);
    m_state.master_vu_left .store(0.f);
    m_state.master_vu_right.store(0.f);
    {
        std::lock_guard<std::mutex> lk(m_state.vu_mutex);
        m_state.vu.fill({});
    }
}

void AudioEngine::setPaused(bool p)         { m_paused.store(p); }
void AudioEngine::seekSeconds(double s)     { m_seek.store(s); }
void AudioEngine::seekToOrder(int32_t o)    { m_seek_order.store(o); }
void AudioEngine::selectSubsong(int32_t i)  { m_subsong.store(i); }
int32_t AudioEngine::subsongCount() const   { return m_mod ? m_mod->get_num_subsongs() : 0; }
openmpt::module_ext* AudioEngine::getModule() const { return m_mod.get(); }

void AudioEngine::playPatternFrom(int32_t order, int32_t startRow, bool loop) {
    if (!m_mod) return;
    m_patPlayLoop  .store(loop);
    m_patPlaySingle.store(false);
    m_patPlayStartRow.store(startRow);
    m_patPlayOrder .store(order); // must be last — triggers callback
    if (!isRunning()) { m_stop.store(false); m_paused.store(false); start(); }
    else              { m_paused.store(false); }
}

void AudioEngine::playRow(int32_t order, int32_t row) {
    if (!m_mod) return;
    m_patPlayLoop  .store(false);
    m_patPlaySingle.store(true);
    m_patPlayStartRow.store(row);
    m_patPlayOrder .store(order); // must be last — triggers callback
    if (!isRunning()) { m_stop.store(false); m_paused.store(false); start(); }
    else              { m_paused.store(false); }
}

int AudioEngine::paCallback(const void*, void* output,
                             unsigned long frameCount,
                             const PaStreamCallbackTimeInfo*,
                             PaStreamCallbackFlags,
                             void* userData)
{
    auto* d   = static_cast<PaCallbackData*>(userData);
    float* out = static_cast<float*>(output);

    if (d->stop->load() || d->paused->load()) {
        std::memset(out, 0, frameCount * 2 * sizeof(float));
        if (d->paused->load()) {
            // Decay VU toward silence while paused
            d->state->master_vu_left .store(d->state->master_vu_left .load() * 0.88f);
            d->state->master_vu_right.store(d->state->master_vu_right.load() * 0.88f);
            int32_t nch = d->state->num_channels.load();
            if (nch > 0) {
                std::lock_guard<std::mutex> lk(d->state->vu_mutex);
                for (int32_t ch = 0; ch < nch && ch < MAX_CHANNELS; ++ch) {
                    d->state->vu[ch].left  *= 0.88f;
                    d->state->vu[ch].right *= 0.88f;
                }
                // Push silence into oscilloscope while paused
                int n = std::min((int)frameCount, WAVE_BUF);
                std::fill(d->state->masterWave.samples,
                          d->state->masterWave.samples + n, 0.f);
                d->state->masterWave.count = n;
            }
        }
        return d->stop->load() ? paAbort : paContinue;
    }

    // Apply pending channel mute changes via the interactive extension (same thread as read())
    uint64_t newMask = d->state->channel_mute_mask.load();
    if (newMask != d->appliedMuteMask) {
        auto* ia = static_cast<openmpt::ext::interactive*>(
            d->mod->get_interface(openmpt::ext::interactive_id));
        if (ia) {
            uint64_t changed = newMask ^ d->appliedMuteMask;
            for (int ch = 0; ch < MAX_CHANNELS; ++ch)
                if ((changed >> ch) & 1)
                    ia->set_channel_mute_status(ch, (newMask >> ch) & 1);
        }
        d->appliedMuteMask = newMask;
    }

    int32_t newSub = d->subsong->exchange(-2);
    if (newSub != -2) d->mod->select_subsong(newSub);

    double seekSec = d->seek->exchange(-1.0);
    if (seekSec >= 0.0) { d->mod->set_position_seconds(seekSec); d->inPatternPlay = false; }

    int32_t seekOrd = d->seek_order->exchange(-1);
    if (seekOrd >= 0) { d->mod->set_position_order_row(seekOrd, 0); d->inPatternPlay = false; }

    // Pattern play: trigger comes from UI thread via pat_play_order atomic
    int32_t ppTrigger = d->pat_play_order->exchange(-1);
    if (ppTrigger >= 0) {
        d->inPatternPlay    = true;
        d->patPlayOrder     = ppTrigger;
        d->patPlayStartRow  = d->pat_play_startrow->load();
        d->patPlayLoop      = d->pat_play_loop->load();
        d->patPlaySingle    = d->pat_play_single->load();
        d->mod->set_position_order_row(ppTrigger, d->patPlayStartRow);
        d->patPrevRow       = d->patPlayStartRow;
    }

    if (d->left.size()  < frameCount) d->left.resize(frameCount);
    if (d->right.size() < frameCount) d->right.resize(frameCount);

    size_t count = d->mod->read(SAMPLE_RATE, frameCount,
                                d->left.data(), d->right.data());

    // Interleave
    for (size_t i = 0; i < count; ++i) {
        out[i * 2]     = d->left[i];
        out[i * 2 + 1] = d->right[i];
    }
    if (count < frameCount)
        std::memset(out + count * 2, 0, (frameCount - count) * 2 * sizeof(float));

    // Pattern play: end-of-pattern detection
    if (d->inPatternPlay && count > 0) {
        int32_t curOrder = d->mod->get_current_order();
        int32_t curRow   = d->mod->get_current_row();

        if (d->patPlaySingle) {
            // Stop when the row advances (row or order changed)
            if (curRow != d->patPrevRow || curOrder != d->patPlayOrder) {
                d->mod->set_position_order_row(d->patPlayOrder, d->patPrevRow);
                d->paused->store(true);
                d->inPatternPlay = false;
            }
        } else {
            // Stop or loop when the pattern boundary is crossed (order changed)
            if (curOrder != d->patPlayOrder) {
                d->mod->set_position_order_row(d->patPlayOrder, d->patPlayStartRow);
                if (!d->patPlayLoop) {
                    d->paused->store(true);
                    d->inPatternPlay = false;
                }
            }
        }
        d->patPrevRow = d->mod->get_current_row();
    }

    // Playback state
    d->state->current_row    .store(d->mod->get_current_row());
    d->state->current_pattern.store(d->mod->get_current_pattern());
    d->state->current_order  .store(d->mod->get_current_order());
    d->state->current_speed  .store(d->mod->get_current_speed());
    d->state->rows_per_beat  .store(d->mod->get_pattern_rows_per_beat(d->mod->get_current_pattern()));
    d->state->position_seconds.store(d->mod->get_position_seconds());
    d->state->bpm            .store(d->mod->get_current_estimated_bpm());

    // Per-channel VU with decay + master oscilloscope snapshot
    int32_t nch = d->state->num_channels.load();
    if (nch > 0) {
        std::lock_guard<std::mutex> lk(d->state->vu_mutex);
        for (int32_t ch = 0; ch < nch && ch < MAX_CHANNELS; ++ch) {
            float rawL = d->mod->get_current_channel_vu_left(ch);
            float rawR = d->mod->get_current_channel_vu_right(ch);
            d->state->vu[ch].left  = d->state->vu[ch].left  * 0.8f + rawL * 0.2f;
            d->state->vu[ch].right = d->state->vu[ch].right * 0.8f + rawR * 0.2f;
        }
        // Snapshot actual signed audio samples for oscilloscope display
        int n = (int)std::min(count, (size_t)WAVE_BUF);
        std::copy(d->left.begin(), d->left.begin() + n, d->state->masterWave.samples);
        d->state->masterWave.count = n;
    }

    // Master VU (peak of interleaved buffer)
    float peakL = 0.0f, peakR = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        peakL = std::max(peakL, std::abs(d->left[i]));
        peakR = std::max(peakR, std::abs(d->right[i]));
    }
    // Instant attack, exponential decay (~300ms to silence at 48kHz/480fpb)
    d->state->master_vu_left .store(std::max(peakL, d->state->master_vu_left .load() * 0.88f));
    d->state->master_vu_right.store(std::max(peakR, d->state->master_vu_right.load() * 0.88f));

    return (count == 0) ? paComplete : paContinue;
}

void AudioEngine::run() {
    if (!m_mod) return;
    m_running.store(true);
    m_state.is_playing.store(true);

    PaCallbackData cbData;
    cbData.mod               = m_mod.get();
    cbData.state             = &m_state;
    cbData.stop              = &m_stop;
    cbData.paused            = &m_paused;
    cbData.seek              = &m_seek;
    cbData.seek_order        = &m_seek_order;
    cbData.subsong           = &m_subsong;
    cbData.pat_play_order    = &m_patPlayOrder;
    cbData.pat_play_startrow = &m_patPlayStartRow;
    cbData.pat_play_loop     = &m_patPlayLoop;
    cbData.pat_play_single   = &m_patPlaySingle;

    Pa_Initialize();

    PaStream* stream = nullptr;
    PaError err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32,
                                       SAMPLE_RATE, FRAMES_BLOCK,
                                       paCallback, &cbData);
    if (err != paNoError) {
        Pa_Terminate();
        m_state.is_playing.store(false);
        m_running.store(false);
        if (onLoadFailed) onLoadFailed(std::string("PortAudio: ") + Pa_GetErrorText(err));
        return;
    }

    Pa_StartStream(stream);
    while (!m_stop.load() && Pa_IsStreamActive(stream) > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    m_state.is_playing.store(false);
    m_running.store(false);
    if (!m_stop.load()) {
        if (onSongFinished) onSongFinished();
    }
}
