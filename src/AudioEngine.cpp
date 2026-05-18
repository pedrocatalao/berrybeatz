#include "AudioEngine.h"
#include <libopenmpt/libopenmpt_ext.hpp>
#include <portaudio.h>
#include <fstream>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>

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
    PaCallbackData()
        : mod(nullptr), state(nullptr), stop(nullptr), paused(nullptr),
          seek(nullptr), seek_order(nullptr), subsong(nullptr) {
        left.resize(FRAMES_BLOCK);
        right.resize(FRAMES_BLOCK);
    }
};

AudioEngine::AudioEngine(ModuleState& state, QObject* parent)
    : QThread(parent), m_state(state) {}

AudioEngine::~AudioEngine() {
    stop();
    wait();
}

bool AudioEngine::loadFile(const QString& path) {
    std::ifstream f(path.toStdString(), std::ios::binary);
    if (!f) { emit loadFailed("Cannot open file: " + path); return false; }
    try {
        m_mod = std::make_unique<openmpt::module_ext>(f);
        fillState();
        return true;
    } catch (const std::exception& e) {
        emit loadFailed(QString("Failed to load module: ") + e.what());
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

void AudioEngine::stop()                    { m_stop.store(true); }

void AudioEngine::rewind() {
    m_stop.store(true);
    wait(); // blocks until audio thread exits (≤50ms)
    if (m_mod) m_mod->set_position_seconds(0.0);
    m_stop.store(false);
    m_paused.store(false);
}
void AudioEngine::setPaused(bool p)         { m_paused.store(p); }
void AudioEngine::seekSeconds(double s)     { m_seek.store(s); }
void AudioEngine::seekToOrder(int32_t o)    { m_seek_order.store(o); }
void AudioEngine::selectSubsong(int32_t i)  { m_subsong.store(i); }
int32_t AudioEngine::subsongCount() const   { return m_mod ? m_mod->get_num_subsongs() : 0; }
openmpt::module_ext* AudioEngine::getModule() const { return m_mod.get(); }

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
        return d->stop->load() ? paAbort : paContinue;
    }

    // Pending seeks / subsong change
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
    if (seekSec >= 0.0) d->mod->set_position_seconds(seekSec);

    int32_t seekOrd = d->seek_order->exchange(-1);
    if (seekOrd >= 0) d->mod->set_position_order_row(seekOrd, 0);

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

    // Playback state
    d->state->current_row    .store(d->mod->get_current_row());
    d->state->current_pattern.store(d->mod->get_current_pattern());
    d->state->current_order  .store(d->mod->get_current_order());
    d->state->current_speed  .store(d->mod->get_current_speed());
    d->state->rows_per_beat  .store(d->mod->get_pattern_rows_per_beat(d->mod->get_current_pattern()));
    d->state->position_seconds.store(d->mod->get_position_seconds());
    d->state->bpm            .store(d->mod->get_current_estimated_bpm());

    // Per-channel VU with decay
    int32_t nch = d->state->num_channels.load();
    if (nch > 0) {
        std::lock_guard<std::mutex> lk(d->state->vu_mutex);
        for (int32_t ch = 0; ch < nch && ch < MAX_CHANNELS; ++ch) {
            d->state->vu[ch].left  = d->state->vu[ch].left  * 0.8f + d->mod->get_current_channel_vu_left(ch)  * 0.2f;
            d->state->vu[ch].right = d->state->vu[ch].right * 0.8f + d->mod->get_current_channel_vu_right(ch) * 0.2f;
        }
    }

    // Master VU (peak of interleaved buffer)
    float peakL = 0.0f, peakR = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        peakL = std::max(peakL, std::abs(d->left[i]));
        peakR = std::max(peakR, std::abs(d->right[i]));
    }
    d->state->master_vu_left .store(d->state->master_vu_left .load() * 0.75f + peakL * 0.25f);
    d->state->master_vu_right.store(d->state->master_vu_right.load() * 0.75f + peakR * 0.25f);

    return (count == 0) ? paComplete : paContinue;
}

void AudioEngine::run() {
    if (!m_mod) return;
    m_stop.store(false);
    m_state.is_playing.store(true);

    PaCallbackData cbData;
    cbData.mod        = m_mod.get();
    cbData.state      = &m_state;
    cbData.stop       = &m_stop;
    cbData.paused     = &m_paused;
    cbData.seek       = &m_seek;
    cbData.seek_order = &m_seek_order;
    cbData.subsong    = &m_subsong;

    Pa_Initialize();

    PaStream* stream = nullptr;
    PaError err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32,
                                       SAMPLE_RATE, FRAMES_BLOCK,
                                       paCallback, &cbData);
    if (err != paNoError) {
        Pa_Terminate();
        m_state.is_playing.store(false);
        emit loadFailed(QString("PortAudio: ") + Pa_GetErrorText(err));
        return;
    }

    Pa_StartStream(stream);
    while (!m_stop.load() && Pa_IsStreamActive(stream) > 0)
        msleep(50);

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    m_state.is_playing.store(false);
    if (!m_stop.load()) emit songFinished();
}
