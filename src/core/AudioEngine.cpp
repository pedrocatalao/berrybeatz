#include "AudioEngine.h"
#include "SoundModule.h"
#include "OpenMPTInternal.h"
#include <portaudio.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <thread>
#include <chrono>

static constexpr int SAMPLE_RATE  = 48000;
static constexpr int FRAMES_BLOCK = 480; // 10ms per block

struct PaCallbackData {
    SoundModule*          mod;
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

    std::atomic<int32_t>* pat_play_order    {nullptr};
    std::atomic<int32_t>* pat_play_startrow {nullptr};
    std::atomic<bool>*    pat_play_loop     {nullptr};
    std::atomic<bool>*    pat_play_single   {nullptr};

    SoundModule* previewMod {nullptr};   // isolated preview module (no song state)

    bool    inPatternPlay   {false};
    int32_t patPlayOrder    {-1};
    int32_t patPlayStartRow {0};
    bool    patPlayLoop     {false};
    bool    patPlaySingle   {false};
    int32_t patPrevRow      {-1};

    // Preview event queue (MPSC; owned by AudioEngine)
    AudioEngine::PreviewEvent* previewQueue {nullptr};
    std::atomic<uint32_t>*     previewHead  {nullptr};
    std::atomic<uint32_t>*     previewTail  {nullptr};
    int32_t previewChannels[129];  // key (0-128) → previewMod channel, audio-thread-only

    std::vector<float> previewLeft;
    std::vector<float> previewRight;

    PaCallbackData()
        : mod(nullptr), state(nullptr), stop(nullptr), paused(nullptr),
          seek(nullptr), seek_order(nullptr), subsong(nullptr) {
        left.resize(FRAMES_BLOCK);
        right.resize(FRAMES_BLOCK);
        previewLeft.resize(FRAMES_BLOCK);
        previewRight.resize(FRAMES_BLOCK);
        std::fill(std::begin(previewChannels), std::end(previewChannels), -1);
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
    auto mod = std::make_unique<SoundModule>();
    if (!mod->loadFile(path)) {
        if (onLoadFailed) onLoadFailed("Cannot open or parse file: " + path);
        return false;
    }
    m_mod = std::move(mod);
    bb_set_ft2_volume_ramping(m_mod.get(), m_ft2VolumeRamping);
    fillState();
    auto prev = std::make_unique<SoundModule>();
    if (prev->newModule(1)) { bb_set_loop_forever(prev.get()); m_previewMod = std::move(prev); }
    return true;
}

bool AudioEngine::newModule(int channels)
{
    auto mod = std::make_unique<SoundModule>();
    if (!mod->newModule(channels)) return false;
    m_mod = std::move(mod);
    bb_set_ft2_volume_ramping(m_mod.get(), m_ft2VolumeRamping);
    fillState();
    auto prev = std::make_unique<SoundModule>();
    if (prev->newModule(1)) { bb_set_loop_forever(prev.get()); m_previewMod = std::move(prev); }
    return true;
}

void AudioEngine::fillState() {
    if (!m_mod) return;
    m_state.gvol_max.store(ModuleState::gvolMaxForType(m_mod->getType()));
    std::lock_guard<std::mutex> lk(m_state.meta_mutex);
    m_state.title             = m_mod->getTitle();
    m_state.artist            = m_mod->getArtist();
    m_state.type_long         = m_mod->getTypeLong();
    m_state.tracker           = m_mod->getTracker();
    m_state.num_patterns      = m_mod->getNumPatterns();
    m_state.num_samples       = m_mod->getNumSamples();
    m_state.num_instruments   = m_mod->getNumInstruments();
    m_state.channel_names     = m_mod->getChannelNames();
    m_state.instrument_names  = m_mod->getInstrumentNames();
    m_state.sample_names      = m_mod->getSampleNames();
    m_state.pattern_names     = m_mod->getPatternNames();
    m_state.num_channels      = m_mod->getNumChannels();
    m_state.num_orders        = m_mod->getNumOrders();
    m_state.duration_seconds  = m_mod->getDurationSeconds();
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
    if (m_mod) m_mod->setPositionSeconds(0.0);
    m_stop.store(false);
    m_paused.store(false);
    m_state.master_vu_left .store(0.f);
    m_state.master_vu_right.store(0.f);
    m_state.vuBuf[0] = {};
    m_state.vuBuf[1] = {};
    m_state.vuWriteIdx.store(0);
}

void AudioEngine::setPaused(bool p) { m_paused.store(p); }

void AudioEngine::startPreview() {
    if (!m_mod) return;
    m_paused.store(true);
    if (!isRunning()) {
        if (m_thread.joinable()) m_thread.join();
        m_stop.store(false);
        m_thread = std::thread(&AudioEngine::run, this);
    }
}

void AudioEngine::stopPreview() {
    m_paused.store(false);
}

void AudioEngine::copyPreviewSample(int idx) {
    if (m_mod && m_previewMod)
        bb_copy_sample(m_previewMod.get(), m_mod.get(), idx);
}

void AudioEngine::copyPreviewInstrument(int instrIdx) {
    if (m_mod && m_previewMod)
        bb_copy_instrument(m_previewMod.get(), m_mod.get(), instrIdx);
}

void AudioEngine::seekSeconds(double s)     { m_seek.store(s); }
void AudioEngine::seekToOrder(int32_t o)    { m_seek_order.store(o); }
void AudioEngine::selectSubsong(int32_t i)  { m_subsong.store(i); }
int32_t AudioEngine::subsongCount() const   { return m_mod ? m_mod->getNumSubsongs() : 0; }
SoundModule* AudioEngine::getModule() const { return m_mod.get(); }

void AudioEngine::setAudioConfig(int deviceIndex, int sampleRate) {
    m_deviceIndex = deviceIndex;
    m_samplerate  = sampleRate;
}

void AudioEngine::setFT2VolumeRamping(bool enabled) {
    m_ft2VolumeRamping = enabled;
    if (m_mod) bb_set_ft2_volume_ramping(m_mod.get(), enabled);
}

std::vector<AudioDeviceInfo> AudioEngine::getDevices() const {
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
    m_patPlayOrder .store(order);
    if (!isRunning()) { m_stop.store(false); m_paused.store(false); start(); }
    else              { m_paused.store(false); }
}

void AudioEngine::playRow(int32_t order, int32_t row) {
    if (!m_mod) return;
    m_patPlayLoop  .store(false);
    m_patPlaySingle.store(true);
    m_patPlayStartRow.store(row);
    m_patPlayOrder .store(order);
    if (!isRunning()) { m_stop.store(false); m_paused.store(false); start(); }
    else              { m_paused.store(false); }
}

void AudioEngine::enqueuePreviewEvent(const PreviewEvent& ev) {
    std::lock_guard<std::mutex> lk(m_previewMutex);  // serialise multiple producers
    uint32_t h = m_previewHead.load(std::memory_order_relaxed);
    uint32_t t = m_previewTail.load(std::memory_order_acquire);
    if (h - t >= static_cast<uint32_t>(kPreviewQueueSize)) return; // full, drop
    m_previewQueue[h & (kPreviewQueueSize - 1)] = ev;
    m_previewHead.store(h + 1, std::memory_order_release);
}

static void drainPreviewQueue(PaCallbackData* d) {
    if (!d->previewQueue || !d->previewMod) return;
    uint32_t t = d->previewTail->load(std::memory_order_relaxed);
    uint32_t h = d->previewHead->load(std::memory_order_acquire);
    while (t != h) {
        const AudioEngine::PreviewEvent& ev =
            d->previewQueue[t & (AudioEngine::kPreviewQueueSize - 1)];
        int key = ev.key;
        if (ev.type == AudioEngine::PreviewEvent::SAMPLE_ON) {
            if (d->previewChannels[key] >= 0)
                d->previewMod->stopNote(d->previewChannels[key]);
            d->previewChannels[key] = bb_play_sample(
                d->previewMod, ev.idx, ev.opnote, ev.vol, ev.pan);
        } else if (ev.type == AudioEngine::PreviewEvent::INSTR_ON) {
            if (d->previewChannels[key] >= 0)
                d->previewMod->stopNote(d->previewChannels[key]);
            d->previewChannels[key] = d->previewMod->playNote(
                ev.idx, ev.opnote, ev.vol, ev.pan);
        } else if (ev.type == AudioEngine::PreviewEvent::NOTE_OFF) {
            if (d->previewChannels[key] >= 0) {
                d->previewMod->stopNote(d->previewChannels[key]);
                d->previewChannels[key] = -1;
            }
        } else { // PITCH_BEND
            for (int k = 0; k <= 128; ++k)
                if (d->previewChannels[k] >= 0)
                    d->previewMod->setNotePitch(d->previewChannels[k], ev.pitch);
        }
        d->previewTail->store(++t, std::memory_order_release);
    }
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

    bool paused = d->paused->load();

    // ── Preview module — always rendered, fully isolated from song state ──────
    drainPreviewQueue(d);
    size_t pc = 0;
    if (d->previewMod)
        pc = d->previewMod->read(d->sampleRate, frameCount,
                                  d->previewLeft.data(), d->previewRight.data());

    if (paused) {
        // Song stopped — output preview only; keep song VU dark.
        for (size_t i = 0; i < frameCount; ++i) {
            out[i * 2]     = (i < pc) ? d->previewLeft[i]  : 0.f;
            out[i * 2 + 1] = (i < pc) ? d->previewRight[i] : 0.f;
        }
        d->state->master_vu_left .store(0.f);
        d->state->master_vu_right.store(0.f);
        {
            int wi = d->state->vuWriteIdx.load(std::memory_order_relaxed);
            d->state->vuBuf[wi] = {};
            d->state->vuWriteIdx.store(wi ^ 1, std::memory_order_release);
        }
        return paContinue;
    }

    // ── Song playback ─────────────────────────────────────────────────────────

    // Apply pending channel mute changes
    uint64_t newMask = d->state->channel_mute_mask.load();
    if (newMask != d->appliedMuteMask) {
        uint64_t changed = newMask ^ d->appliedMuteMask;
        for (int ch = 0; ch < MAX_CHANNELS; ++ch)
            if ((changed >> ch) & 1)
                d->mod->setChannelMute(ch, (newMask >> ch) & 1);
        d->appliedMuteMask = newMask;
    }

    int32_t newSub = d->subsong->exchange(-2);
    if (newSub != -2) d->mod->selectSubsong(newSub);

    double seekSec = d->seek->exchange(-1.0);
    if (seekSec >= 0.0) { d->mod->setPositionSeconds(seekSec); d->inPatternPlay = false; }

    int32_t seekOrd = d->seek_order->exchange(-1);
    if (seekOrd >= 0) { d->mod->setPositionOrderRow(seekOrd, 0); d->inPatternPlay = false; }

    // Pattern play trigger
    int32_t ppTrigger = d->pat_play_order->exchange(-1);
    if (ppTrigger >= 0) {
        d->inPatternPlay    = true;
        d->patPlayOrder     = ppTrigger;
        d->patPlayStartRow  = d->pat_play_startrow->load();
        d->patPlayLoop      = d->pat_play_loop->load();
        d->patPlaySingle    = d->pat_play_single->load();
        d->mod->setPositionOrderRow(ppTrigger, d->patPlayStartRow);
        d->patPrevRow = d->patPlayStartRow;
    }

    int32_t nchForCapture = std::min(d->state->num_channels.load(), (int32_t)MAX_CHANNELS);
    if (nchForCapture > 0)
        bb_setup_channel_capture(d->mod, nchForCapture);

    size_t count = d->mod->read(d->sampleRate, frameCount,
                                d->left.data(), d->right.data());
    d->mod->advancePositionSeconds((double)count / d->sampleRate);

    if (count == 0) {
        // Song finished — pause and rewind; stream stays alive for preview notes.
        d->paused->store(true);
        d->mod->setPositionSeconds(0.0);
        d->inPatternPlay = false;
        for (size_t i = 0; i < frameCount; ++i) {
            out[i * 2]     = (i < pc) ? d->previewLeft[i]  : 0.f;
            out[i * 2 + 1] = (i < pc) ? d->previewRight[i] : 0.f;
        }
        d->state->master_vu_left .store(0.f);
        d->state->master_vu_right.store(0.f);
        {
            int wi = d->state->vuWriteIdx.load(std::memory_order_relaxed);
            d->state->vuBuf[wi] = {};
            d->state->vuWriteIdx.store(wi ^ 1, std::memory_order_release);
        }
        return paContinue;
    }

    // Mix song + preview
    for (size_t i = 0; i < count; ++i) {
        out[i * 2]     = d->left[i]  + ((i < pc) ? d->previewLeft[i]  : 0.f);
        out[i * 2 + 1] = d->right[i] + ((i < pc) ? d->previewRight[i] : 0.f);
    }
    for (size_t i = count; i < frameCount; ++i) {
        out[i * 2]     = (i < pc) ? d->previewLeft[i]  : 0.f;
        out[i * 2 + 1] = (i < pc) ? d->previewRight[i] : 0.f;
    }

    // Pattern play: end-of-pattern detection
    if (d->inPatternPlay && count > 0) {
        int32_t curOrder = d->mod->getCurrentOrder();
        int32_t curRow   = d->mod->getCurrentRow();

        if (d->patPlaySingle) {
            if (curRow != d->patPrevRow || curOrder != d->patPlayOrder) {
                d->mod->setPositionOrderRow(d->patPlayOrder, d->patPrevRow);
                d->paused->store(true);
                d->inPatternPlay = false;
            }
        } else {
            if (curOrder != d->patPlayOrder) {
                d->mod->setPositionOrderRow(d->patPlayOrder, d->patPlayStartRow);
                if (!d->pat_play_loop->load()) {
                    d->paused->store(true);
                    d->inPatternPlay = false;
                }
            }
        }
        d->patPrevRow = d->mod->getCurrentRow();
    }

    // Playback state
    d->state->current_row    .store(d->mod->getCurrentRow());
    d->state->current_pattern.store(d->mod->getCurrentPattern());
    d->state->current_order  .store(d->mod->getCurrentOrder());
    d->state->current_speed  .store(d->mod->getCurrentSpeed());
    d->state->rows_per_beat  .store(d->mod->getPatternRowsPerBeat(d->mod->getCurrentPattern()));
    d->state->position_seconds.store(d->mod->getPositionSeconds());
    d->state->bpm            .store(d->mod->getCurrentBPM());

    // Per-channel VU + waveform captures
    int32_t nch = std::min(d->state->num_channels.load(), (int32_t)MAX_CHANNELS);
    if (nch > 0) {
        static thread_local float   chanBuf[MAX_CHANNELS * WAVE_BUF];
        static thread_local int32_t chanCnt[MAX_CHANNELS];
        bb_capture_channel_waves(d->mod, chanBuf, chanCnt, nch, FRAMES_BLOCK);

        // Ratio normalisation: scale per-channel captures to match actual output amplitude
        {
            int ns = chanCnt[0];
            for (int s = 0; s < ns; ++s) {
                float total = 0.f;
                for (int ch = 0; ch < nch; ++ch)
                    total += chanBuf[ch * FRAMES_BLOCK + s];
                if (std::abs(total) > 1e-9f) {
                    float outSamp = d->left[s];
                    for (int ch = 0; ch < nch; ++ch)
                        chanBuf[ch * FRAMES_BLOCK + s] = outSamp * chanBuf[ch * FRAMES_BLOCK + s] / total;
                } else {
                    for (int ch = 0; ch < nch; ++ch)
                        chanBuf[ch * FRAMES_BLOCK + s] = 0.f;
                }
            }
        }

        bb_apply_opl_channel_waves(d->mod, chanBuf, nch, FRAMES_BLOCK);

        int wi = d->state->vuWriteIdx.load(std::memory_order_relaxed);
        VuSnapshot& snap = d->state->vuBuf[wi];
        for (int32_t ch = 0; ch < nch && ch < MAX_CHANNELS; ++ch) {
            float rawL = d->mod->getChannelVuLeft(ch);
            float rawR = d->mod->getChannelVuRight(ch);
            snap.vu[ch].left  = std::max(rawL, snap.vu[ch].left  * 0.88f);
            snap.vu[ch].right = std::max(rawR, snap.vu[ch].right * 0.88f);

            int wn = std::min(chanCnt[ch], WAVE_BUF);
            std::copy(chanBuf + ch * FRAMES_BLOCK, chanBuf + ch * FRAMES_BLOCK + wn,
                      snap.channelWave[ch].samples);
            snap.channelWave[ch].count = wn;
        }
        int n = (int)std::min(count, (size_t)WAVE_BUF);
        std::copy(d->left.begin(), d->left.begin() + n, snap.masterWave.samples);
        snap.masterWave.count = n;
        d->state->vuWriteIdx.store(wi ^ 1, std::memory_order_release);
    }

    // Master VU from mixed output
    float peakL = 0.0f, peakR = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        peakL = std::max(peakL, std::abs(out[i * 2]));
        peakR = std::max(peakR, std::abs(out[i * 2 + 1]));
    }
    d->state->master_vu_left .store(std::max(peakL, d->state->master_vu_left .load() * 0.88f));
    d->state->master_vu_right.store(std::max(peakR, d->state->master_vu_right.load() * 0.88f));

    return paContinue;
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
    cbData.previewMod        = m_previewMod.get();
    cbData.sampleRate        = m_samplerate;
    cbData.previewQueue = m_previewQueue;
    cbData.previewHead  = &m_previewHead;
    cbData.previewTail  = &m_previewTail;

    auto tryOpen = [&](PaStream*& stream) -> PaError {
        const PaDeviceInfo* info = nullptr;
        PaDeviceIndex dev = (m_deviceIndex >= 0)
            ? (PaDeviceIndex)m_deviceIndex
            : Pa_GetDefaultOutputDevice();
        info = Pa_GetDeviceInfo(dev);
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

    static constexpr int kMaxRetries = 5;
    int retriesLeft = kMaxRetries;

    while (!m_stop.load() && retriesLeft > 0) {
        PaStream* stream = nullptr;
        PaError   err    = tryOpen(stream);
        if (err != paNoError) {
            if (retriesLeft == kMaxRetries)
                if (onLoadFailed) onLoadFailed(std::string("PortAudio: ") + Pa_GetErrorText(err));
            --retriesLeft;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            continue;
        }
        retriesLeft = kMaxRetries;
        Pa_StartStream(stream);
        while (!m_stop.load() && !m_restartStream.load() && Pa_IsStreamActive(stream) > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        if (!m_stop.load()) {
            m_restartStream.store(false);
            Pa_Terminate();
            Pa_Initialize();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    m_state.master_vu_left .store(0.f);
    m_state.master_vu_right.store(0.f);
    m_state.vuBuf[0] = {};
    m_state.vuBuf[1] = {};
    m_state.vuWriteIdx.store(0);
    m_state.is_playing.store(false);
    m_running.store(false);
    if (!m_stop.load()) {
        if (onSongFinished) onSongFinished();
    }
}
