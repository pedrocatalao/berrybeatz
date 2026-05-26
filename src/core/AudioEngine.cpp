#include "AudioEngine.h"
#include "OpenMPTInternal.h"
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
    int                   sampleRate{48000};

    // Pattern play trigger pointers (owned by AudioEngine)
    std::atomic<int32_t>* pat_play_order    {nullptr};
    std::atomic<int32_t>* pat_play_startrow {nullptr};
    std::atomic<bool>*    pat_play_loop     {nullptr};
    std::atomic<bool>*    pat_play_single   {nullptr};
    std::atomic<bool>*    preview_mode      {nullptr};

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
    : m_state(state)
    , m_deviceMonitor([this]() { if (isRunning()) m_restartStream.store(true); })
{
    Pa_Initialize();
}

AudioEngine::~AudioEngine() {
    stop();
    join();
    Pa_Terminate();
}

bool AudioEngine::loadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (onLoadFailed) onLoadFailed("Cannot open file: " + path);
        return false;
    }
    try {
        m_mod = std::make_unique<openmpt::module_ext>(f);
        bb_set_ft2_volume_ramping(m_mod.get(), m_ft2VolumeRamping);
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
    if (m_thread.joinable()) m_thread.join();
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

void AudioEngine::startPreview() {
    if (!m_mod) return;
    m_previewMode.store(true);
    if (!isRunning()) {
        if (m_thread.joinable()) m_thread.join();
        m_stop.store(false);
        m_paused.store(true);   // start paused so normal song playback doesn't begin
        m_thread = std::thread(&AudioEngine::run, this);
    }
    // If already running (paused or playing), previewMode flag is enough.
    // Playing: read() already called, notes play normally.
    // Paused:  callback will now enter the preview branch.
}

void AudioEngine::stopPreview() {
    m_previewMode.store(false);
}

void AudioEngine::seekSeconds(double s)     { m_seek.store(s); }
void AudioEngine::seekToOrder(int32_t o)    { m_seek_order.store(o); }
void AudioEngine::selectSubsong(int32_t i)  { m_subsong.store(i); }
int32_t AudioEngine::subsongCount() const   { return m_mod ? m_mod->get_num_subsongs() : 0; }
openmpt::module_ext* AudioEngine::getModule() const { return m_mod.get(); }

void AudioEngine::setAudioConfig(int deviceIndex, int sampleRate)
{
    m_deviceIndex = deviceIndex;
    m_samplerate  = sampleRate;
}

void AudioEngine::setFT2VolumeRamping(bool enabled)
{
    m_ft2VolumeRamping = enabled;
    if (m_mod)
        bb_set_ft2_volume_ramping(m_mod.get(), enabled);
}

std::vector<AudioDeviceInfo> AudioEngine::getDevices() const
{
    std::vector<AudioDeviceInfo> result;
    int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info || info->maxOutputChannels < 2) continue;
        result.push_back({i, info->name, info->maxOutputChannels, info->defaultSampleRate});
    }
    return result;
}

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

    if (d->stop->load()) {
        std::memset(out, 0, frameCount * 2 * sizeof(float));
        return paAbort;
    }

    bool paused  = d->paused->load();
    bool preview = d->preview_mode && d->preview_mode->load();

    if (paused && !preview) {
        std::memset(out, 0, frameCount * 2 * sizeof(float));
        d->state->master_vu_left .store(0.f);
        d->state->master_vu_right.store(0.f);
        {
            std::lock_guard<std::mutex> lk(d->state->vu_mutex);
            d->state->vu.fill({});
            d->state->masterWave = {};
        }
        return paContinue;
    }

    if (paused && preview) {
        // Note-preview mode: mute all song channels so only play_note()
        // interactive channels contribute to the output.
        int nc = std::min(d->state->num_channels.load(), (int32_t)MAX_CHANNELS);
        auto* ia = static_cast<openmpt::ext::interactive*>(
            d->mod->get_interface(openmpt::ext::interactive_id));
        if (ia) {
            for (int ch = 0; ch < nc; ++ch)
                ia->set_channel_mute_status(ch, true);
        }

        if (d->left.size()  < frameCount) d->left.resize(frameCount);
        if (d->right.size() < frameCount) d->right.resize(frameCount);

        size_t count = d->mod->read(d->sampleRate, frameCount,
                                    d->left.data(), d->right.data());

        if (ia) {
            for (int ch = 0; ch < nc; ++ch) {
                bool userMuted = (d->appliedMuteMask >> ch) & 1;
                ia->set_channel_mute_status(ch, userMuted);
            }
        }

        if (count == 0)
            d->mod->set_position_seconds(0.0);  // loop if module ends during preview

        for (size_t i = 0; i < count; ++i) {
            out[i * 2]     = d->left[i];
            out[i * 2 + 1] = d->right[i];
        }
        if (count < frameCount)
            std::memset(out + count * 2, 0, (frameCount - count) * 2 * sizeof(float));

        return paContinue;
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

    int32_t nchForCapture = d->state->num_channels.load();
    if (nchForCapture > 0)
        bb_setup_channel_capture(d->mod, nchForCapture);

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
                if (!d->pat_play_loop->load()) {
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

    // Per-channel VU + waveform captures + master oscilloscope snapshot
    int32_t nch = d->state->num_channels.load();
    if (nch > 0) {
        static thread_local float   chanBuf[MAX_CHANNELS * WAVE_BUF];
        static thread_local int32_t chanCnt[MAX_CHANNELS];
        bb_capture_channel_waves(d->mod, chanBuf, chanCnt, nch, FRAMES_BLOCK);

        // Scale per-channel captures to match actual output amplitude.
        // All post-processing (global volume, DSP, reverb) applies a uniform transform to
        // MixSoundBuffer. Each channel's fraction of the total is preserved, so:
        //   channelOut[ch][s] = d->left[s] * (rawCapture[ch][s] / sumCapture[s])
        // This is exact for linear mixing with no guessing about internal scale factors.
        {
            int ns = chanCnt[0];
            for (int s = 0; s < ns; ++s) {
                float total = 0.f;
                for (int ch = 0; ch < nch; ++ch)
                    total += chanBuf[ch * FRAMES_BLOCK + s];
                if (std::abs(total) > 1e-9f) {
                    float out = d->left[s];
                    for (int ch = 0; ch < nch; ++ch)
                        chanBuf[ch * FRAMES_BLOCK + s] = out * chanBuf[ch * FRAMES_BLOCK + s] / total;
                } else {
                    for (int ch = 0; ch < nch; ++ch)
                        chanBuf[ch * FRAMES_BLOCK + s] = 0.f;
                }
            }
        }

        std::lock_guard<std::mutex> lk(d->state->vu_mutex);
        for (int32_t ch = 0; ch < nch && ch < MAX_CHANNELS; ++ch) {
            float rawL = d->mod->get_current_channel_vu_left(ch);
            float rawR = d->mod->get_current_channel_vu_right(ch);
            d->state->vu[ch].left  = std::max(rawL, d->state->vu[ch].left  * 0.88f);
            d->state->vu[ch].right = std::max(rawR, d->state->vu[ch].right * 0.88f);

            int wn = std::min(chanCnt[ch], WAVE_BUF);
            std::copy(chanBuf + ch * FRAMES_BLOCK, chanBuf + ch * FRAMES_BLOCK + wn,
                      d->state->channelWave[ch].samples);
            d->state->channelWave[ch].count = wn;
        }
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

    // Refresh PortAudio's device list — stale after plug/unplug since Pa_Initialize()
    Pa_Terminate();
    Pa_Initialize();

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
    cbData.preview_mode      = &m_previewMode;
    cbData.sampleRate        = m_samplerate;

    auto tryOpen = [&](PaStream*& stream) -> PaError {
        const PaDeviceInfo* info = nullptr;
        PaDeviceIndex dev = (m_deviceIndex >= 0)
            ? (PaDeviceIndex)m_deviceIndex
            : Pa_GetDefaultOutputDevice();
        info = Pa_GetDeviceInfo(dev);

        // Fall back to default if the configured device is gone
        if (!info && m_deviceIndex >= 0) {
            dev  = Pa_GetDefaultOutputDevice();
            info = Pa_GetDeviceInfo(dev);
        }
        if (!info) return paInvalidDevice;

        PaStreamParameters params{};
        params.device                    = dev;
        params.channelCount              = 2;
        params.sampleFormat              = paFloat32;
        params.suggestedLatency          = info->defaultLowOutputLatency;
        params.hostApiSpecificStreamInfo = nullptr;
        return Pa_OpenStream(&stream, nullptr, &params,
                             m_samplerate, FRAMES_BLOCK, paClipOff, paCallback, &cbData);
    };

    // Retry loop — recovers from device disconnects (e.g. headphone plug/unplug)
    static constexpr int kMaxRetries = 5;
    int retriesLeft = kMaxRetries;

    while (!m_stop.load() && retriesLeft > 0) {
        PaStream* stream = nullptr;
        PaError   err    = tryOpen(stream);

        if (err != paNoError) {
            if (retriesLeft == kMaxRetries)   // first attempt — report error
                if (onLoadFailed) onLoadFailed(std::string("PortAudio: ") + Pa_GetErrorText(err));
            --retriesLeft;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            continue;
        }
        retriesLeft = kMaxRetries;  // reset on successful open

        Pa_StartStream(stream);
        while (!m_stop.load() && !m_restartStream.load() && Pa_IsStreamActive(stream) > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

        Pa_StopStream(stream);
        Pa_CloseStream(stream);

        if (!m_stop.load()) {
            // Device changed or stream died — reinit PA, wait for OS to settle, then retry.
            m_restartStream.store(false);
            Pa_Terminate();
            Pa_Initialize();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    // Clear visual state now that audio has fully stopped
    m_state.master_vu_left .store(0.f);
    m_state.master_vu_right.store(0.f);
    {
        std::lock_guard<std::mutex> lk(m_state.vu_mutex);
        m_state.vu.fill({});
        m_state.masterWave = {};
    }

    m_state.is_playing.store(false);
    m_running.store(false);
    if (!m_stop.load()) {
        if (onSongFinished) onSongFinished();
    }
}
