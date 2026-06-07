// SoundModule.cpp — direct OpenMPT::CSoundFile wrapper.
// Compiled with LIBOPENMPT_BUILD + soundlib include paths (see CMakeLists.txt).
// BERRYBEATZ_FILESAVE: enables Save* methods without enabling ENABLE_TESTS.
#define BERRYBEATZ_FILESAVE

#include "common/stdafx.h"
#include "soundlib/Sndfile.h"
#include "soundlib/patternContainer.h"
#include "soundlib/pattern.h"
#include "soundlib/OPL.h"
#include "soundlib/AudioReadTarget.h"
#include "soundlib/mod_specifications.h"
#include "common/FileReader.h"

#include "SoundModule.h"
#include "PatternEditor.h"

#include <fstream>
#include <cmath>
#include <algorithm>
#include <limits>

// ── IAudioTarget implementation ───────────────────────────────────────────────
// Writes CSoundFile output into separate L/R float buffers (planar stereo).
// No dither needed for float output.

namespace {

struct PlanarStereoTarget : public OpenMPT::IAudioTarget {
    float*      m_left;
    float*      m_right;
    std::size_t m_capacity;
    std::size_t m_written = 0;
    float       m_gain;

    PlanarStereoTarget(float* left, float* right, std::size_t cap, float gain)
        : m_left(left), m_right(right), m_capacity(cap), m_gain(gain) {}

    std::size_t written() const { return m_written; }

    // Integer mix path (fixed-point → float)
    void Process(mpt::audio_span_interleaved<OpenMPT::MixSampleInt> buf) override {
        const std::size_t frames = std::min(buf.size_frames(), m_capacity - m_written);
        constexpr float kScale = 1.0f / static_cast<float>(1 << OpenMPT::MixSampleIntTraits::mix_fractional_bits);
        for (std::size_t f = 0; f < frames; ++f) {
            m_left [m_written + f] = static_cast<float>(buf(0, f)) * kScale * m_gain;
            m_right[m_written + f] = (buf.size_channels() > 1)
                ? static_cast<float>(buf(1, f)) * kScale * m_gain
                : m_left[m_written + f];
        }
        m_written += frames;
    }

    // Float mix path (direct copy + gain)
    void Process(mpt::audio_span_interleaved<OpenMPT::MixSampleFloat> buf) override {
        const std::size_t frames = std::min(buf.size_frames(), m_capacity - m_written);
        for (std::size_t f = 0; f < frames; ++f) {
            m_left [m_written + f] = buf(0, f) * m_gain;
            m_right[m_written + f] = (buf.size_channels() > 1)
                ? buf(1, f) * m_gain
                : buf(0, f) * m_gain;
        }
        m_written += frames;
    }
};

// Helpers copied from libopenmpt_impl.cpp (needed for render param mapping)

static OpenMPT::ResamplingMode filterlength_to_resamplingmode(int length) {
    if (length >= 8) return OpenMPT::SRCMODE_SINC8LP;
    if (length >= 3) return OpenMPT::SRCMODE_CUBIC;
    if (length >= 2) return OpenMPT::SRCMODE_LINEAR;
    if (length >= 1) return OpenMPT::SRCMODE_NEAREST;
    return OpenMPT::SRCMODE_SINC8LP;
}

static int resamplingmode_to_filterlength(OpenMPT::ResamplingMode mode) {
    switch (mode) {
        case OpenMPT::SRCMODE_NEAREST:  return 1;
        case OpenMPT::SRCMODE_LINEAR:   return 2;
        case OpenMPT::SRCMODE_CUBIC:    return 4;
        default:                         return 8;
    }
}

static void ramping_to_mixersettings(OpenMPT::MixerSettings& s, int ramping) {
    if (ramping == -1) {
        s.SetVolumeRampUpMicroseconds  (OpenMPT::MixerSettings().GetVolumeRampUpMicroseconds());
        s.SetVolumeRampDownMicroseconds(OpenMPT::MixerSettings().GetVolumeRampDownMicroseconds());
    } else if (ramping <= 0) {
        s.SetVolumeRampUpMicroseconds  (0);
        s.SetVolumeRampDownMicroseconds(0);
    } else {
        s.SetVolumeRampUpMicroseconds  (ramping * 1000);
        s.SetVolumeRampDownMicroseconds(ramping * 1000);
    }
}

static int mixersettings_to_ramping(const OpenMPT::MixerSettings& s) {
    std::int32_t ramp_us = std::max(s.GetVolumeRampUpMicroseconds(), s.GetVolumeRampDownMicroseconds());
    const OpenMPT::MixerSettings def;
    if (s.GetVolumeRampUpMicroseconds()   == def.GetVolumeRampUpMicroseconds() &&
        s.GetVolumeRampDownMicroseconds() == def.GetVolumeRampDownMicroseconds())
        return -1;
    if (ramp_us <= 0) return 0;
    return (ramp_us + 500) / 1000;
}

} // anonymous namespace

// ── SoundModule::Impl ─────────────────────────────────────────────────────────

struct SoundModule::Impl {
    OpenMPT::CSoundFile sf;
    bool mixerInitialized = false;
};

// ── SoundModule ───────────────────────────────────────────────────────────────

SoundModule::SoundModule()
    : m_impl(std::make_unique<Impl>())
{}

SoundModule::~SoundModule() = default;

void* SoundModule::getCSFRaw() { return &m_impl->sf; }

bool SoundModule::loadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    // Read entire file into memory so we can construct a FileReader from a span
    f.seekg(0, std::ios::end);
    const std::streamsize fileSize = f.tellg();
    f.seekg(0);
    std::vector<std::byte> rawData(static_cast<std::size_t>(fileSize));
    f.read(reinterpret_cast<char*>(rawData.data()), fileSize);
    f.close();

    // Destroy existing state
    m_impl->sf.Destroy();
    m_impl->mixerInitialized = false;
    m_loaded    = false;
    m_posSeconds = 0.0;
    m_subsongs.clear();
    m_subsong = 0;

    OpenMPT::FileReader fileCursor(mpt::as_span(rawData));
    if (!m_impl->sf.Create(fileCursor, OpenMPT::CSoundFile::loadCompleteModule))
        return false;

    m_loaded = true;
    computeSubsongs();
    return true;
}

bool SoundModule::newModule(int channels) {
    m_impl->sf.Destroy();
    m_impl->mixerInitialized = false;
    m_loaded     = false;
    m_posSeconds = 0.0;
    m_subsongs.clear();
    m_subsong = 0;

    auto& sf = m_impl->sf;
    sf.InitializeGlobals(OpenMPT::MOD_TYPE_IT, static_cast<OpenMPT::CHANNELINDEX>(channels));
    sf.m_PlayState.m_nMusicTempo = OpenMPT::TEMPO(125, 0);
    sf.m_PlayState.m_nMusicSpeed = 6;
    sf.m_nDefaultGlobalVolume    = OpenMPT::MAX_GLOBAL_VOLUME;
    sf.m_SongFlags.set(OpenMPT::SONG_ITOLDEFFECTS);
    sf.m_SongFlags.set(OpenMPT::SONG_ITCOMPATGXX);
    sf.m_songName = "Untitled";

    // One empty 64-row pattern, order 0 → pattern 0
    sf.Patterns.Insert(0, 64);
    sf.Order().assign(1, 0);

    m_loaded = true;
    computeSubsongs();
    return true;
}

void SoundModule::computeSubsongs() {
    m_subsongs.clear();
    auto& sf = m_impl->sf;
    OpenMPT::SEQUENCEINDEX nseq = sf.Order.GetNumSequences();
    if (nseq == 0) {
        m_subsongs.push_back({0.0, 0, 0, 0});
        return;
    }
    for (OpenMPT::SEQUENCEINDEX seq = 0; seq < nseq; ++seq) {
        auto results = sf.GetLength(OpenMPT::eNoAdjust,
            OpenMPT::GetLengthTarget(false).StartPos(seq, 0, 0));
        if (!results.empty()) {
            const auto& r = results.back();
            m_subsongs.push_back({r.duration, (int)r.startOrder, (int)r.startRow, (int)seq});
        }
    }
    if (m_subsongs.empty())
        m_subsongs.push_back({0.0, 0, 0, 0});
}

void SoundModule::initMixer(int samplerate, int channels) {
    auto& sf = m_impl->sf;
    auto settings = sf.m_MixerSettings;
    bool changed = (int)settings.gdwMixingFreq != samplerate ||
                   (int)settings.gnChannels    != channels;
    if (changed || !m_impl->mixerInitialized) {
        auto volRampUp   = settings.GetVolumeRampUpMicroseconds();
        auto volRampDown = settings.GetVolumeRampDownMicroseconds();
        settings.gdwMixingFreq = samplerate;
        settings.gnChannels    = channels;
        settings.SetVolumeRampUpMicroseconds  (volRampUp);
        settings.SetVolumeRampDownMicroseconds(volRampDown);
        sf.SetMixerSettings(settings);
    }
    if (!m_impl->mixerInitialized) {
        sf.InitPlayer(true);
    }
    m_impl->mixerInitialized = true;
}

std::size_t SoundModule::read(int samplerate, std::size_t count, float* left, float* right) {
    if (!m_loaded || !left || !right || count == 0) return 0;

    initMixer(samplerate, 2);

    auto& sf = m_impl->sf;
    sf.ResetMixStat();
    sf.m_bIsRendering = true;

    PlanarStereoTarget target(left, right, count, m_masterGain);
    std::size_t remaining = count;
    while (remaining > 0) {
        std::size_t chunk = sf.Read(
            static_cast<OpenMPT::samplecount_t>(
                std::min((std::uint64_t)remaining,
                         (std::uint64_t)(std::numeric_limits<OpenMPT::samplecount_t>::max() / 8))),
            target);
        if (chunk == 0) break;
        remaining -= chunk;
    }
    return target.written();
}

// ── Metadata ──────────────────────────────────────────────────────────────────

std::string SoundModule::getTitle() const {
    return m_impl->sf.GetTitle();
}

std::string SoundModule::getArtist() const {
    return mpt::transcode<std::string>(mpt::common_encoding::utf8, m_impl->sf.m_songArtist);
}

std::string SoundModule::getType() const {
    return mpt::transcode<std::string>(mpt::common_encoding::utf8, m_impl->sf.m_modFormat.type);
}

std::string SoundModule::getTypeLong() const {
    return mpt::transcode<std::string>(mpt::common_encoding::utf8, m_impl->sf.m_modFormat.formatName);
}

std::string SoundModule::getTracker() const {
    return mpt::transcode<std::string>(mpt::common_encoding::utf8, m_impl->sf.m_modFormat.madeWithTracker);
}

std::string SoundModule::getMessage() const {
    return m_impl->sf.m_songMessage.GetFormatted(OpenMPT::SongMessage::leLF);
}

int SoundModule::getNumChannels() const {
    return (int)m_impl->sf.GetNumChannels();
}

int SoundModule::getNumOrders() const {
    return (int)m_impl->sf.Order().GetLengthTailTrimmed();
}

int SoundModule::getNumPatterns() const {
    return (int)m_impl->sf.Patterns.GetNumPatterns();
}

int SoundModule::getNumSamples() const {
    return (int)m_impl->sf.GetNumSamples();
}

int SoundModule::getNumInstruments() const {
    return (int)m_impl->sf.GetNumInstruments();
}

int SoundModule::getNumSubsongs() const {
    return (int)m_subsongs.size();
}

double SoundModule::getDurationSeconds() const {
    if (!m_loaded) return 0.0;
    auto& sf = m_impl->sf;
    auto results = sf.GetLength(OpenMPT::eNoAdjust);
    return results.empty() ? 0.0 : results.back().duration;
}

std::vector<std::string> SoundModule::getChannelNames() const {
    auto& sf = m_impl->sf;
    int nc = (int)sf.GetNumChannels();
    std::vector<std::string> names;
    names.reserve(nc);
    for (int i = 0; i < nc; ++i)
        names.push_back(sf.ChnSettings[i].szName);
    return names;
}

std::vector<std::string> SoundModule::getSampleNames() const {
    auto& sf = m_impl->sf;
    int ns = (int)sf.GetNumSamples();
    std::vector<std::string> names;
    names.reserve(ns);
    for (int i = 1; i <= ns; ++i) {
        const char* n = sf.GetSampleName(i);
        names.push_back(n ? n : "");
    }
    return names;
}

std::vector<std::string> SoundModule::getInstrumentNames() const {
    auto& sf = m_impl->sf;
    int ni = (int)sf.GetNumInstruments();
    std::vector<std::string> names;
    names.reserve(ni);
    for (int i = 1; i <= ni; ++i)
        names.push_back(sf.Instruments[i] ? sf.Instruments[i]->GetName() : "");
    return names;
}

std::vector<std::string> SoundModule::getPatternNames() const {
    auto& sf = m_impl->sf;
    int np = (int)sf.Patterns.GetNumPatterns();
    std::vector<std::string> names;
    names.reserve(np);
    for (int i = 0; i < np; ++i)
        names.push_back(sf.Patterns.IsValidPat(i) ? sf.Patterns[i].GetName() : "");
    return names;
}

// ── Position ──────────────────────────────────────────────────────────────────

double SoundModule::setPositionSeconds(double seconds) {
    if (!m_loaded) return 0.0;
    auto& sf = m_impl->sf;

    const SubsongData* sub = &m_subsongs[0];
    double base = 0.0;
    if (m_subsong == 0 && m_subsongs.size() > 1) {
        // When "all subsongs" play sequentially, find which one contains this time
        for (const auto& s : m_subsongs) {
            if (base + s.duration > seconds) { sub = &s; break; }
            base += s.duration;
        }
    } else if (m_subsong < (int)m_subsongs.size()) {
        sub = &m_subsongs[m_subsong];
    }

    double seek = seconds - base;
    sf.SetCurrentOrder(static_cast<OpenMPT::ORDERINDEX>(sub->startOrder));
    auto result = sf.GetLength(OpenMPT::eAdjust,
        OpenMPT::GetLengthTarget(seek).StartPos(
            static_cast<OpenMPT::SEQUENCEINDEX>(sub->seq),
            static_cast<OpenMPT::ORDERINDEX>(sub->startOrder),
            static_cast<OpenMPT::ROWINDEX>(sub->startRow))).back();

    sf.m_PlayState.m_nNextOrder = sf.m_PlayState.m_nCurrentOrder =
        result.targetReached ? result.restartOrder : result.endOrder;
    sf.m_PlayState.m_nNextRow   = result.targetReached ? result.restartRow : result.endRow;
    sf.m_PlayState.m_nTickCount = OpenMPT::CSoundFile::TICKS_ROW_FINISHED;
    m_posSeconds = base + result.duration;
    return m_posSeconds;
}

double SoundModule::setPositionOrderRow(int order, int row) {
    if (!m_loaded) return m_posSeconds;
    auto& sf = m_impl->sf;
    int numOrders = (int)sf.Order().GetLengthTailTrimmed();
    if (order < 0 || order >= numOrders) return m_posSeconds;

    OpenMPT::PATTERNINDEX pat = sf.Order()[order];
    if (sf.Patterns.IsValidIndex(pat)) {
        if (row < 0 || row >= (int)sf.Patterns[pat].GetNumRows()) return m_posSeconds;
    } else {
        row = 0;
    }

    sf.m_PlayState.m_nCurrentOrder = static_cast<OpenMPT::ORDERINDEX>(order);
    sf.SetCurrentOrder(static_cast<OpenMPT::ORDERINDEX>(order));
    sf.m_PlayState.m_nNextRow   = static_cast<OpenMPT::ROWINDEX>(row);
    sf.m_PlayState.m_nTickCount = OpenMPT::CSoundFile::TICKS_ROW_FINISHED;

    auto result = sf.GetLength(OpenMPT::eAdjust,
        OpenMPT::GetLengthTarget(
            static_cast<OpenMPT::ORDERINDEX>(order),
            static_cast<OpenMPT::ROWINDEX>(row))).back();
    m_posSeconds = result.duration;
    return m_posSeconds;
}

// ── Real-time state ───────────────────────────────────────────────────────────

int SoundModule::getCurrentRow() const {
    return (int)m_impl->sf.m_PlayState.m_nRow;
}

int SoundModule::getCurrentOrder() const {
    return (int)m_impl->sf.GetCurrentOrder();
}

int SoundModule::getCurrentPattern() const {
    return (int)m_impl->sf.GetCurrentPattern();
}

int SoundModule::getCurrentSpeed() const {
    return (int)m_impl->sf.GetMusicSpeed();
}

double SoundModule::getCurrentBPM() const {
    return m_impl->sf.GetCurrentBPM();
}

double SoundModule::getCurrentTempo2() const {
    return m_impl->sf.m_PlayState.m_nMusicTempo.ToDouble();
}

int SoundModule::getPatternRowsPerBeat(int pattern) const {
    auto& sf = m_impl->sf;
    if (!sf.Patterns.IsValidPat(pattern)) return 4;
    int rpb = (int)sf.Patterns[pattern].GetRowsPerBeat();
    return rpb > 0 ? rpb : (int)sf.m_PlayState.m_nCurrentRowsPerBeat;
}

int SoundModule::getPatternNumRows(int pattern) const {
    auto& sf = m_impl->sf;
    if (!sf.Patterns.IsValidPat(pattern)) return 0;
    return (int)sf.Patterns[pattern].GetNumRows();
}

int SoundModule::getPatternRowsPerMeasure(int pattern) const {
    auto& sf = m_impl->sf;
    if (!sf.Patterns.IsValidPat(pattern)) return 16;
    int rpm = (int)sf.Patterns[pattern].GetRowsPerMeasure();
    if (rpm > 0) return rpm;
    return getPatternRowsPerBeat(pattern) * 4;
}

int SoundModule::getOrderPattern(int order) const {
    auto& sf = m_impl->sf;
    auto& seq = sf.Order();
    if (order < 0 || order >= (int)seq.GetLengthTailTrimmed()) return -1;
    OpenMPT::PATTERNINDEX p = seq[order];
    return sf.Patterns.IsValidPat(p) ? (int)p : -1;
}

float SoundModule::getChannelVuLeft(int ch) const {
    auto& sf = m_impl->sf;
    if (ch < 0 || ch >= (int)sf.GetNumChannels()) return 0.f;
    auto& chn = sf.m_PlayState.Chn[ch];
    return chn.dwFlags[OpenMPT::CHN_SURROUND] ? 0.f : chn.nLeftVU * (1.f / 128.f);
}

float SoundModule::getChannelVuRight(int ch) const {
    auto& sf = m_impl->sf;
    if (ch < 0 || ch >= (int)sf.GetNumChannels()) return 0.f;
    auto& chn = sf.m_PlayState.Chn[ch];
    return chn.dwFlags[OpenMPT::CHN_SURROUND] ? 0.f : chn.nRightVU * (1.f / 128.f);
}

// ── Subsong ───────────────────────────────────────────────────────────────────

void SoundModule::selectSubsong(int index) {
    if (index < 0 || index >= (int)m_subsongs.size()) return;
    m_subsong = index;
    const auto& sub = m_subsongs[index];
    auto& sf = m_impl->sf;
    sf.Order.SetSequence(static_cast<OpenMPT::SEQUENCEINDEX>(sub.seq));
    sf.SetCurrentOrder(static_cast<OpenMPT::ORDERINDEX>(sub.startOrder));
    sf.m_PlayState.m_nNextRow   = static_cast<OpenMPT::ROWINDEX>(sub.startRow);
    sf.m_PlayState.m_nTickCount = OpenMPT::CSoundFile::TICKS_ROW_FINISHED;
    m_posSeconds = 0.0;
}

// ── Mute ──────────────────────────────────────────────────────────────────────

void SoundModule::setChannelMute(int channel, bool muted) {
    auto& sf = m_impl->sf;
    if (channel < 0 || channel >= (int)sf.GetNumChannels()) return;
    sf.ChnSettings[channel].dwFlags.set(OpenMPT::CHN_MUTE | OpenMPT::CHN_SYNCMUTE, muted);
    sf.m_PlayState.Chn[channel].dwFlags.set(OpenMPT::CHN_MUTE | OpenMPT::CHN_SYNCMUTE, muted);
    // Also update NNA channels
    for (OpenMPT::CHANNELINDEX i = sf.GetNumChannels(); i < OpenMPT::MAX_CHANNELS; ++i) {
        if (sf.m_PlayState.Chn[i].nMasterChn == (OpenMPT::CHANNELINDEX)(channel + 1))
            sf.m_PlayState.Chn[i].dwFlags.set(OpenMPT::CHN_MUTE | OpenMPT::CHN_SYNCMUTE, muted);
    }
}

// ── Transport ─────────────────────────────────────────────────────────────────

void SoundModule::setTempo(double bpm) {
    if (bpm < 32.0 || bpm > 512.0) return;
    m_impl->sf.m_PlayState.m_nMusicTempo = decltype(m_impl->sf.m_PlayState.m_nMusicTempo)(bpm);
}

void SoundModule::setSpeed(int speed) {
    if (speed < 1 || speed > 65535) return;
    m_impl->sf.m_PlayState.m_nMusicSpeed = speed;
}

void SoundModule::setGlobalVolume(double factor) {
    m_impl->sf.m_PlayState.m_nGlobalVolume =
        mpt::saturate_round<uint32_t>(std::max(0.0, std::min(1.0, factor)) * OpenMPT::MAX_GLOBAL_VOLUME);
}

double SoundModule::getGlobalVolume() const {
    return m_impl->sf.m_PlayState.m_nGlobalVolume / static_cast<double>(OpenMPT::MAX_GLOBAL_VOLUME);
}

void SoundModule::setTempoFactor(double factor) {
    if (factor <= 0.0 || factor > 4.0) return;
    m_impl->sf.m_nTempoFactor = mpt::saturate_round<uint32_t>(65536.0 / factor);
    m_impl->sf.RecalculateSamplesPerTick();
}

double SoundModule::getTempoFactor() const {
    return 65536.0 / m_impl->sf.m_nTempoFactor;
}

void SoundModule::setPitchFactor(double factor) {
    if (factor <= 0.0 || factor > 4.0) return;
    m_impl->sf.m_nFreqFactor = mpt::saturate_round<uint32_t>(65536.0 * factor);
    m_impl->sf.RecalculateSamplesPerTick();
}

double SoundModule::getPitchFactor() const {
    return m_impl->sf.m_nFreqFactor / 65536.0;
}

// ── Render params ─────────────────────────────────────────────────────────────

int SoundModule::getRenderParam(RenderParam param) const {
    auto& sf = m_impl->sf;
    switch (param) {
        case RENDER_MASTERGAIN_MILLIBEL:
            return static_cast<int>(1000.f * 2.f * std::log10(m_masterGain));
        case RENDER_STEREOSEPARATION_PERCENT:
            return (int)(sf.m_MixerSettings.m_nStereoSeparation * 100
                         / OpenMPT::MixerSettings::StereoSeparationScale);
        case RENDER_INTERPOLATIONFILTER_LENGTH:
            return resamplingmode_to_filterlength(sf.m_Resampler.m_Settings.SrcMode);
        case RENDER_VOLUMERAMPING_STRENGTH:
            return mixersettings_to_ramping(sf.m_MixerSettings);
        default: return 0;
    }
}

void SoundModule::setRenderParam(RenderParam param, int value) {
    auto& sf = m_impl->sf;
    switch (param) {
        case RENDER_MASTERGAIN_MILLIBEL:
            m_masterGain = std::pow(10.f, (float)value * 0.001f * 0.5f);
            break;
        case RENDER_STEREOSEPARATION_PERCENT: {
            auto newSep = (int32_t)(value * OpenMPT::MixerSettings::StereoSeparationScale / 100);
            if (newSep != (int32_t)sf.m_MixerSettings.m_nStereoSeparation) {
                auto settings = sf.m_MixerSettings;
                settings.m_nStereoSeparation = newSep;
                sf.SetMixerSettings(settings);
            }
            break;
        }
        case RENDER_INTERPOLATIONFILTER_LENGTH: {
            auto newSettings = sf.m_Resampler.m_Settings;
            newSettings.SrcMode = filterlength_to_resamplingmode(value);
            if (newSettings != sf.m_Resampler.m_Settings)
                sf.SetResamplerSettings(newSettings);
            break;
        }
        case RENDER_VOLUMERAMPING_STRENGTH: {
            auto newSettings = sf.m_MixerSettings;
            ramping_to_mixersettings(newSettings, value);
            if (sf.m_MixerSettings.VolumeRampUpMicroseconds   != newSettings.VolumeRampUpMicroseconds ||
                sf.m_MixerSettings.VolumeRampDownMicroseconds != newSettings.VolumeRampDownMicroseconds)
                sf.SetMixerSettings(newSettings);
            break;
        }
        default: break;
    }
}

// ── Note preview ──────────────────────────────────────────────────────────────

int SoundModule::playNote(int instrument, int note, double volume, double panning) {
    auto& sf = m_impl->sf;
    const bool instrMode = sf.GetNumInstruments() > 0;
    const int maxInstr = instrMode ? (int)sf.GetNumInstruments() : (int)sf.GetNumSamples();
    if (instrument < 0 || instrument >= maxInstr) return -1;

    // note is 0-based from libopenmpt's perspective; add NOTE_MIN to get internal note
    note += OpenMPT::NOTE_MIN;
    if (note < OpenMPT::NOTE_MIN || note > OpenMPT::NOTE_MAX) return -1;

    OpenMPT::CHANNELINDEX ch = sf.GetNNAChannel(OpenMPT::CHANNELINDEX_INVALID);
    if (ch == OpenMPT::CHANNELINDEX_INVALID) ch = OpenMPT::MAX_CHANNELS - 1;

    OpenMPT::ModChannel& chn = sf.m_PlayState.Chn[ch];
    chn.Reset(OpenMPT::ModChannel::resetTotal, sf, OpenMPT::CHANNELINDEX_INVALID,
              static_cast<OpenMPT::ChannelFlags>(OpenMPT::CHN_MUTE));
    chn.nMasterChn = 0;
    chn.nNewNote = chn.nLastNote = static_cast<uint8_t>(note);
    chn.ResetEnvelopes();
    sf.InstrumentChange(chn, instrument + 1);
    chn.nFadeOutVol = 0x10000;
    sf.NoteChange(chn, note, false, true, true);
    chn.nPan    = mpt::saturate_round<int32_t>(std::clamp(panning * 128.0, -128.0, 128.0) + 128.0);
    chn.nVolume = mpt::saturate_round<int32_t>(std::clamp(volume  * 256.0,   0.0, 256.0));

    // Remove channel from mix list (fix stale-channel issue)
    auto mb = std::begin(sf.m_PlayState.ChnMix);
    auto me = std::remove(mb, mb + sf.m_nMixChannels, ch);
    sf.m_nMixChannels = static_cast<OpenMPT::CHANNELINDEX>(std::distance(mb, me));

    return (int)ch;
}

void SoundModule::stopNote(int channel) {
    if (channel < 0 || channel >= OpenMPT::MAX_CHANNELS) return;
    auto& chn = m_impl->sf.m_PlayState.Chn[channel];
    chn.nLength        = 0;
    chn.pCurrentSample = nullptr;
}

bool SoundModule::releaseFinishedChannel(int channel) {
    if (channel < 0 || channel >= OpenMPT::MAX_CHANNELS) return true;
    auto& sf  = m_impl->sf;
    // If the channel is still in the mix it hasn't finished yet.
    const auto* mb = std::begin(sf.m_PlayState.ChnMix);
    const auto* me = mb + sf.m_nMixChannels;
    if (std::find(mb, me, static_cast<OpenMPT::CHANNELINDEX>(channel)) != me)
        return false;
    // Channel left the mix naturally — zero fadeout so GetNNAChannel can reuse it.
    sf.m_PlayState.Chn[channel].nFadeOutVol = 0;
    return true;
}

void SoundModule::setNotePitch(int channel, double semitones) {
    if (channel < 0 || channel >= OpenMPT::MAX_CHANNELS) return;
    m_impl->sf.m_PlayState.Chn[channel].microTuning =
        mpt::saturate_round<int16_t>(semitones * 32768.0);
}

// ── Pattern data ──────────────────────────────────────────────────────────────

void SoundModule::getPatternCell(int pat, int row, int ch, PatternCell& out) const {
    auto& sf = m_impl->sf;
    if (!sf.Patterns.IsValidPat(pat)) return;
    const OpenMPT::ModCommand* c = sf.Patterns[pat].GetpModCommand(row, ch);
    if (!c) return;
    out.note    = c->note;
    out.instr   = c->instr;
    out.volcmd  = static_cast<uint8_t>(c->volcmd);
    out.vol     = c->vol;
    out.command = static_cast<uint8_t>(c->command);
    out.param   = c->param;
}

void SoundModule::setPatternCell(int pat, int row, int ch, const PatternCell& in) {
    auto& sf = m_impl->sf;
    if (!sf.Patterns.IsValidPat(pat)) return;
    OpenMPT::ModCommand* c = sf.Patterns[pat].GetpModCommand(row, ch);
    if (!c) return;
    c->note    = in.note;
    c->instr   = in.instr;
    c->volcmd  = static_cast<OpenMPT::VolumeCommand>(in.volcmd);
    c->vol     = in.vol;
    c->command = static_cast<OpenMPT::EffectCommand>(in.command);
    c->param   = in.param;
}

void SoundModule::clearPatternCell(int pat, int row, int ch) {
    auto& sf = m_impl->sf;
    if (!sf.Patterns.IsValidPat(pat)) return;
    OpenMPT::ModCommand* c = sf.Patterns[pat].GetpModCommand(row, ch);
    if (!c) return;
    *c = OpenMPT::ModCommand{};
}

std::string SoundModule::formatCellCommand(int pat, int row, int ch, CellCommand cmd) const {
    auto& sf = m_impl->sf;
    if (!sf.Patterns.IsValidPat(pat)) return {};
    const OpenMPT::CPattern& pattern = sf.Patterns[pat];
    if (row < 0 || row >= (int)pattern.GetNumRows()) return {};
    if (ch < 0 || ch >= (int)sf.GetNumChannels()) return {};

    const OpenMPT::ModCommand& cell = *pattern.GetpModCommand(
        static_cast<OpenMPT::ROWINDEX>(row),
        static_cast<OpenMPT::CHANNELINDEX>(ch));

    switch (cmd) {
        case COMMAND_NOTE:
            return (cell.IsNote() || cell.IsSpecialNote())
                ? mpt::transcode<std::string>(mpt::common_encoding::utf8,
                      sf.GetNoteName(cell.note, cell.instr))
                : std::string("...");

        case COMMAND_INSTRUMENT:
            return cell.instr ? OpenMPT::mpt::afmt::HEX0<2>(cell.instr)
                              : std::string("..");

        case COMMAND_VOLUMEEFFECT:
            return cell.IsPcNote()
                ? std::string(" ")
                : std::string(1, OpenMPT::CModSpecifications::GetGenericVolEffectLetter(cell.volcmd));

        case COMMAND_VOLUME:
            if (cell.IsPcNote())
                return OpenMPT::mpt::afmt::HEX0<2>(cell.GetValueVolCol() & 0xff);
            return cell.volcmd != OpenMPT::VOLCMD_NONE
                ? OpenMPT::mpt::afmt::HEX0<2>(cell.vol)
                : std::string("..");

        case COMMAND_EFFECT:
            if (cell.IsPcNote())
                return OpenMPT::mpt::afmt::HEX0<1>((cell.GetValueEffectCol() & 0x0f00) >> 8);
            return cell.command != OpenMPT::CMD_NONE
                ? std::string(1, sf.GetModSpecifications().GetEffectLetter(cell.command))
                : std::string(".");

        case COMMAND_PARAMETER:
            if (cell.IsPcNote())
                return OpenMPT::mpt::afmt::HEX0<2>(cell.GetValueEffectCol() & 0x00ff);
            return cell.command != OpenMPT::CMD_NONE
                ? OpenMPT::mpt::afmt::HEX0<2>(cell.param)
                : std::string("..");

        default: return {};
    }
}
