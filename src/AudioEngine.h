#pragma once
#include "ModuleState.h"
#include <QThread>
#include <QString>
#include <memory>
#include <atomic>

namespace openmpt { class module; class module_ext; }
struct PaStreamCallbackTimeInfo;
typedef unsigned long PaStreamCallbackFlags;
typedef void PaStream;

class AudioEngine : public QThread {
    Q_OBJECT
public:
    explicit AudioEngine(ModuleState& state, QObject* parent = nullptr);
    ~AudioEngine() override;

    bool loadFile(const QString& path);
    void stop();
    void rewind(); // stop audio thread, rewind to start, keep module loaded
    void setPaused(bool paused);
    bool isPaused() const { return m_paused.load(); }
    void seekSeconds(double seconds);
    void seekToOrder(int32_t order);
    void selectSubsong(int32_t index);
    int32_t subsongCount() const;
    openmpt::module_ext* getModule() const;

signals:
    void loadFailed(const QString& reason);
    void songFinished();

protected:
    void run() override;

private:
    static int paCallback(const void* input, void* output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);

    void fillState();

    ModuleState&                    m_state;
    std::unique_ptr<openmpt::module_ext> m_mod;
    PaStream*                        m_stream  {nullptr};
    std::atomic<bool>                m_stop    {false};
    std::atomic<bool>                m_paused  {false};
    std::atomic<double>              m_seek       {-1.0};
    std::atomic<int32_t>             m_seek_order {-1};
    std::atomic<int32_t>             m_subsong    {-2}; // -2 = no change
    int32_t                          m_samplerate{48000};
};
