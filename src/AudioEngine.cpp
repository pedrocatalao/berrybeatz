#include "AudioEngine.h"
#include <libopenmpt/libopenmpt.hpp>
#include <portaudio.h>
#include <fstream>
#include <vector>
#include <cstring>

static constexpr int SAMPLE_RATE   = 48000;
static constexpr int FRAMES_BLOCK  = 480; // 10ms per block

struct PaCallbackData {
    openmpt::module* mod;
    ModuleState*     state;
    std::atomic<bool>*   stop;
    std::atomic<bool>*   paused;
    std::atomic<double>* seek;
    std::atomic<int32_t>* subsong;
    std::vector<float>   left;
    std::vector<float>   right;

    PaCallbackData()
        : mod(nullptr), state(nullptr), stop(nullptr),
          paused(nullptr), seek(nullptr), subsong(nullptr) {
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
    if (!f) {
        emit loadFailed("Cannot open file: " + path);
        return false;
    }
    try {
        auto mod = std::make_unique<openmpt::module>(f);
        // Swap in on the main thread before run() starts
        m_mod = std::move(mod);
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
    m_state.title            = m_mod->get_metadata("title");
    m_state.artist           = m_mod->get_metadata("artist");
    m_state.type_long        = m_mod->get_metadata("type_long");
    m_state.tracker          = m_mod->get_metadata("tracker");
    m_state.num_patterns     = m_mod->get_num_patterns();
    m_state.num_samples      = m_mod->get_num_samples();
    m_state.num_instruments  = m_mod->get_num_instruments();
    m_state.channel_names    = m_mod->get_channel_names();
    m_state.instrument_names = m_mod->get_instrument_names();
    m_state.sample_names     = m_mod->get_sample_names();
    m_state.num_channels     = m_mod->get_num_channels();
    m_state.num_orders       = m_mod->get_num_orders();
    m_state.duration_seconds = m_mod->get_duration_seconds();
}

void AudioEngine::stop() {
    m_stop.store(true);
}

void AudioEngine::setPaused(bool paused) {
    m_paused.store(paused);
}

void AudioEngine::seekSeconds(double seconds) {
    m_seek.store(seconds);
}

void AudioEngine::selectSubsong(int32_t index) {
    m_subsong.store(index);
}

int32_t AudioEngine::subsongCount() const {
    return m_mod ? m_mod->get_num_subsongs() : 0;
}

openmpt::module* AudioEngine::getModule() const {
    return m_mod.get();
}

int AudioEngine::paCallback(const void* /*input*/, void* output,
                             unsigned long frameCount,
                             const PaStreamCallbackTimeInfo* /*timeInfo*/,
                             PaStreamCallbackFlags /*statusFlags*/,
                             void* userData)
{
    auto* d = static_cast<PaCallbackData*>(userData);

    if (d->stop->load() || d->paused->load()) {
        std::memset(output, 0, frameCount * 2 * sizeof(float));
        return d->stop->load() ? paAbort : paContinue;
    }

    // Handle subsong change
    int32_t newSubsong = d->subsong->exchange(-2);
    if (newSubsong != -2)
        d->mod->select_subsong(newSubsong);

    // Handle seek
    double seekTo = d->seek->exchange(-1.0);
    if (seekTo >= 0.0)
        d->mod->set_position_seconds(seekTo);

    // Ensure buffers are large enough
    if (d->left.size() < frameCount)  d->left.resize(frameCount);
    if (d->right.size() < frameCount) d->right.resize(frameCount);

    size_t count = d->mod->read(SAMPLE_RATE, frameCount,
                                d->left.data(), d->right.data());

    // Interleave into PortAudio buffer
    float* out = static_cast<float*>(output);
    for (size_t i = 0; i < count; ++i) {
        out[i * 2]     = d->left[i];
        out[i * 2 + 1] = d->right[i];
    }
    if (count < frameCount) {
        std::memset(out + count * 2, 0, (frameCount - count) * 2 * sizeof(float));
    }

    // Update shared state
    d->state->current_row     .store(d->mod->get_current_row());
    d->state->current_pattern .store(d->mod->get_current_pattern());
    d->state->current_order   .store(d->mod->get_current_order());
    d->state->current_speed   .store(d->mod->get_current_speed());
    d->state->position_seconds.store(d->mod->get_position_seconds());
    d->state->bpm             .store(d->mod->get_current_estimated_bpm());

    int32_t nch = d->state->num_channels.load();
    if (nch > 0) {
        std::lock_guard<std::mutex> lk(d->state->vu_mutex);
        for (int32_t ch = 0; ch < nch && ch < MAX_CHANNELS; ++ch) {
            // Smooth decay
            d->state->vu[ch].left  = d->state->vu[ch].left  * 0.8f + d->mod->get_current_channel_vu_left(ch)  * 0.2f;
            d->state->vu[ch].right = d->state->vu[ch].right * 0.8f + d->mod->get_current_channel_vu_right(ch) * 0.2f;
        }
    }

    return (count == 0) ? paComplete : paContinue;
}

void AudioEngine::run() {
    if (!m_mod) return;
    m_stop.store(false);
    m_state.is_playing.store(true);

    PaCallbackData cbData;
    cbData.mod     = m_mod.get();
    cbData.state   = &m_state;
    cbData.stop    = &m_stop;
    cbData.paused  = &m_paused;
    cbData.seek    = &m_seek;
    cbData.subsong = &m_subsong;

    Pa_Initialize();

    PaStream* stream = nullptr;
    PaError err = Pa_OpenDefaultStream(
        &stream,
        0, 2,            // no input, 2-channel output
        paFloat32,
        SAMPLE_RATE,
        FRAMES_BLOCK,
        paCallback,
        &cbData
    );

    if (err != paNoError) {
        Pa_Terminate();
        m_state.is_playing.store(false);
        emit loadFailed(QString("PortAudio error: ") + Pa_GetErrorText(err));
        return;
    }

    Pa_StartStream(stream);

    // Wait until the stream finishes or we're told to stop
    while (!m_stop.load()) {
        PaError active = Pa_IsStreamActive(stream);
        if (active <= 0) break;
        msleep(50);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    m_state.is_playing.store(false);
    if (!m_stop.load())
        emit songFinished();
}
