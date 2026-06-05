#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

struct PatternCell;  // defined in PatternEditor.h

// Opaque wrapper around OpenMPT::CSoundFile.
// SoundModule.cpp is compiled with LIBOPENMPT_BUILD + soundlib include paths.
// All other TUs include only this header — no soundlib headers needed.
class SoundModule {
public:
    SoundModule();
    ~SoundModule();
    SoundModule(const SoundModule&) = delete;
    SoundModule& operator=(const SoundModule&) = delete;

    bool loadFile(const std::string& path);
    bool newModule(int channels = 8);   // create empty IT-based module (.btz)
    bool isLoaded() const { return m_loaded; }

    // Render param IDs (mirrors openmpt::module::RENDER_* values)
    enum RenderParam {
        RENDER_MASTERGAIN_MILLIBEL        = 1,
        RENDER_STEREOSEPARATION_PERCENT   = 2,
        RENDER_INTERPOLATIONFILTER_LENGTH = 3,
        RENDER_VOLUMERAMPING_STRENGTH     = 4,
    };

    // Format command IDs (mirrors openmpt::module::command_* values)
    enum CellCommand {
        COMMAND_NOTE        = 0,
        COMMAND_INSTRUMENT  = 1,
        COMMAND_VOLUMEEFFECT = 2,
        COMMAND_VOLUME      = 3,
        COMMAND_EFFECT      = 4,
        COMMAND_PARAMETER   = 5,
    };

    // ── Mixer setup ────────────────────────────────────────────────────────────
    // Must be called before first read() and whenever samplerate changes.
    void initMixer(int samplerate, int channels = 2);

    // Render frames into planar L/R float buffers.
    // Returns number of frames rendered (0 = end of song).
    std::size_t read(int samplerate, std::size_t count, float* left, float* right);

    // ── Metadata ───────────────────────────────────────────────────────────────
    std::string getTitle() const;
    std::string getArtist() const;
    std::string getType() const;       // e.g. "xm", "it", "mptm"
    std::string getTypeLong() const;
    std::string getTracker() const;
    std::string getMessage() const;

    int getNumChannels() const;
    int getNumOrders() const;
    int getNumPatterns() const;
    int getNumSamples() const;
    int getNumInstruments() const;
    int getNumSubsongs() const;

    double getDurationSeconds() const;

    std::vector<std::string> getChannelNames() const;
    std::vector<std::string> getSampleNames() const;
    std::vector<std::string> getInstrumentNames() const;
    std::vector<std::string> getPatternNames() const;

    // ── Position ───────────────────────────────────────────────────────────────
    double setPositionSeconds(double seconds);
    double setPositionOrderRow(int order, int row);
    double getPositionSeconds() const { return m_posSeconds; }
    void   advancePositionSeconds(double delta) { m_posSeconds += delta; }

    // ── Real-time playback state ───────────────────────────────────────────────
    int    getCurrentRow() const;
    int    getCurrentOrder() const;
    int    getCurrentPattern() const;
    int    getCurrentSpeed() const;
    double getCurrentBPM() const;       // estimated BPM (same as libopenmpt)
    double getCurrentTempo2() const;    // music tempo as double BPM
    int    getPatternRowsPerBeat(int pattern) const;
    int    getPatternRowsPerMeasure(int pattern) const;
    int    getPatternNumRows(int pattern) const;
    int    getOrderPattern(int order) const;  // returns -1 for skip/invalid
    float  getChannelVuLeft(int ch) const;
    float  getChannelVuRight(int ch) const;

    // ── Subsong ────────────────────────────────────────────────────────────────
    void selectSubsong(int index);

    // ── Channel mute ──────────────────────────────────────────────────────────
    void setChannelMute(int channel, bool muted);

    // ── Transport control ─────────────────────────────────────────────────────
    void   setTempo(double bpm);
    void   setSpeed(int speed);
    void   setGlobalVolume(double factor);  // 0.0–1.0
    double getGlobalVolume() const;
    void   setTempoFactor(double factor);
    double getTempoFactor() const;
    void   setPitchFactor(double factor);
    double getPitchFactor() const;

    // ── Render parameters ─────────────────────────────────────────────────────
    int  getRenderParam(RenderParam param) const;
    void setRenderParam(RenderParam param, int value);

    // ── Note preview ──────────────────────────────────────────────────────────
    int  playNote(int instrument, int note, double volume, double panning);
    void stopNote(int channel);
    void setNotePitch(int channel, double semitones);
    // Returns true and reclaims the slot if the channel has left the mix naturally.
    bool releaseFinishedChannel(int channel);

    // ── Pattern data ──────────────────────────────────────────────────────────
    void getPatternCell(int pat, int row, int ch, PatternCell& out) const;
    void setPatternCell(int pat, int row, int ch, const PatternCell& in);
    void clearPatternCell(int pat, int row, int ch);

    // Format a single cell command field as text (for tracker display)
    std::string formatCellCommand(int pat, int row, int ch, CellCommand cmd) const;

    // ── Raw CSoundFile access (for bb_* functions in SoundFileOps.cpp) ──────
    // Returns OpenMPT::CSoundFile* — cast at call site.
    void* getCSFRaw();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool   m_loaded     = false;
    double m_posSeconds = 0.0;
    float  m_masterGain = 1.0f;  // linear gain factor for RENDER_MASTERGAIN_MILLIBEL

    struct SubsongData {
        double duration;
        int    startOrder;
        int    startRow;
        int    seq;
    };
    std::vector<SubsongData> m_subsongs;
    int m_subsong = 0;

    void computeSubsongs();
};
