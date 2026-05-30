#pragma once
#include "ModuleState.h"
#include "SoundModule.h"
#include "../platform/AudioDeviceMonitor.h"
#include <thread>
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>

struct PaStreamCallbackTimeInfo;
typedef unsigned long PaStreamCallbackFlags;
typedef void PaStream;

struct AudioDeviceInfo {
    int         index;
    std::string name;
    int         maxOutputChannels;
    double      defaultSampleRate;
};

class AudioEngine {
public:
    explicit AudioEngine(ModuleState& state);
    ~AudioEngine();

    bool    loadFile(const std::string& path);
    bool    newModule(int channels = 8);

    void    start();            // launch audio thread
    void    stop();             // signal thread to stop (non-blocking)
    void    join();             // wait for thread to exit
    bool    isRunning() const;

    void    rewind();           // stop + join, rewind to start, keep module loaded

    void    setPaused(bool paused);
    bool    isPaused() const { return m_paused.load(); }

    // Note-preview mode: runs the audio loop with song channels muted so only
    // play_note() interactive channels are audible. Safe to call when stopped.
    void    startPreview();
    void    stopPreview();

    void    seekSeconds(double seconds);
    void    seekToOrder(int32_t order);
    void    selectSubsong(int32_t index);
    int32_t subsongCount() const;

    SoundModule* getModule() const;

    // Audio device configuration (takes effect on next start())
    void setAudioConfig(int deviceIndex, int sampleRate);
    void setFT2VolumeRamping(bool enabled);
    std::vector<AudioDeviceInfo> getDevices() const;

    // Pattern-specific playback
    void    playPatternFrom(int32_t order, int32_t startRow, bool loop);
    void    playRow(int32_t order, int32_t row);
    void    setLoopPattern(bool loop) { m_patPlayLoop.store(loop); }

    // Callbacks (set before start(); called from audio thread)
    std::function<void(const std::string&)> onLoadFailed;
    std::function<void()>                   onSongFinished;

    // ── Preview note queue (SPSC, lock-free) ─────────────────────────────────
    // Enqueue from any thread; audio callback drains before each read().
    // key: 0-127 = MIDI/keyboard note slot, 128 = click-play slot.
    struct PreviewEvent {
        enum Type : uint8_t { SAMPLE_ON, INSTR_ON, NOTE_OFF, PITCH_BEND } type;
        uint8_t  key;       // SAMPLE_ON / INSTR_ON / NOTE_OFF
        int32_t  idx;       // SAMPLE_ON: sampleIdx (1-based); INSTR_ON: instrIdx (0-based)
        int32_t  opnote;    // SAMPLE_ON / INSTR_ON: 1-based OpenMPT note
        float    vol;
        float    pan;
        float    pitch;     // PITCH_BEND only (semitones)
    };
    static constexpr int kPreviewQueueSize = 64; // must be power of 2
    void enqueuePreviewEvent(const PreviewEvent& ev);

private:
    static int paCallback(const void* input, void* output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);

    void run();
    void fillState();

    ModuleState&                         m_state;
    std::unique_ptr<SoundModule> m_mod;

    std::thread              m_thread;
    std::atomic<bool>        m_running {false};
    std::atomic<bool>        m_stop    {false};
    std::atomic<bool>        m_paused  {false};
    bool                     m_ft2VolumeRamping {false};
    std::atomic<double>      m_seek       {-1.0};
    std::atomic<int32_t>     m_seek_order {-1};
    std::atomic<int32_t>     m_subsong    {-2};   // -2 = no change
    int32_t                  m_samplerate  {48000};
    int                      m_deviceIndex {-1};

    // Pattern play — written by UI thread, read+consumed by callback
    std::atomic<int32_t>     m_patPlayOrder    {-1}; // -1 = idle, >=0 = trigger
    std::atomic<int32_t>     m_patPlayStartRow {0};
    std::atomic<bool>        m_patPlayLoop     {false};
    std::atomic<bool>        m_patPlaySingle   {false};

    std::atomic<bool>        m_previewMode     {false};
    std::atomic<bool>        m_restartStream   {false};
    AudioDeviceMonitor       m_deviceMonitor;

    // SPSC queue — producer: UI/MIDI thread, consumer: audio callback
    PreviewEvent              m_previewQueue[kPreviewQueueSize];
    std::atomic<uint32_t>     m_previewHead {0};  // written by producer
    std::atomic<uint32_t>     m_previewTail {0};  // written by consumer
};
