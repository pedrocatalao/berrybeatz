// This TU accesses OpenMPT soundlib internals.
// LIBOPENMPT_BUILD is set via compile definitions (CMakeLists).
// BERRYBEATZ_FILESAVE prevents BuildSettings.h from setting MODPLUG_NO_FILESAVE,
// making Save* method declarations visible in Sndfile.h.
// The actual save symbols are provided by libopenmpt.a (built with -DBERRYBEATZ_FILESAVE).
#define BERRYBEATZ_FILESAVE

// stdafx.h is the canonical OpenMPT prelude: brings in all common types,
// ILog, LogLevel, charbuf, mpt::String etc. that soundlib headers rely on.
#include "common/stdafx.h"

#include "soundlib/Sndfile.h"
#include "soundlib/patternContainer.h"
#include "soundlib/pattern.h"
#include "soundlib/OPL.h"

#include "OpenMPTInternal.h"

#include <fstream>
#include <string>
#include <cstring>
#include <cmath>

// Provided by OpenMPTInternalGet.cpp — extracts CSoundFile* via libopenmpt
// internal cast (kept in a separate TU to avoid header namespace conflict).
void* bb_get_soundfile_ptr(openmpt::module_ext* mod);

static OpenMPT::CSoundFile& getSF(openmpt::module_ext* mod) {
    return *static_cast<OpenMPT::CSoundFile*>(bb_get_soundfile_ptr(mod));
}

bool bb_save_module(openmpt::module_ext* mod, const std::string& path, const std::string& type) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    auto& sf = getSF(mod);
    if (type == "xm")  return sf.SaveXM(f);
    if (type == "s3m") return sf.SaveS3M(f);
    if (type == "mod") return sf.SaveMod(f);
    // IT and MPTM: pass empty PathString (embedded samples only)
    return sf.SaveIT(f, OpenMPT::mpt::PathString{});
}

int bb_insert_pattern(openmpt::module_ext* mod, int rows) {
    auto& sf = getSF(mod);
    OpenMPT::PATTERNINDEX idx = sf.Patterns.InsertAny(
        static_cast<OpenMPT::ROWINDEX>(rows));
    if (idx == OpenMPT::PATTERNINDEX_INVALID) return -1;
    return static_cast<int>(idx);
}

// ─── Instrument helpers ───────────────────────────────────────────────────────

static void copyEnvData(InstrumentEnvData& dst, const OpenMPT::InstrumentEnvelope& src) {
    dst.nodes.clear();
    for (const auto& n : src)
        dst.nodes.push_back({n.tick, n.value});
    dst.enabled  = src.dwFlags[OpenMPT::ENV_ENABLED];
    dst.loop     = src.dwFlags[OpenMPT::ENV_LOOP];
    dst.sustain  = src.dwFlags[OpenMPT::ENV_SUSTAIN];
    dst.carry    = src.dwFlags[OpenMPT::ENV_CARRY];
    dst.filter   = src.dwFlags[OpenMPT::ENV_FILTER];
    dst.loopStart    = src.nLoopStart;
    dst.loopEnd      = src.nLoopEnd;
    dst.sustainStart = src.nSustainStart;
    dst.sustainEnd   = src.nSustainEnd;
}

static void writeEnvData(OpenMPT::InstrumentEnvelope& dst, const InstrumentEnvData& src) {
    dst.clear();
    for (const auto& n : src.nodes)
        dst.push_back(n.tick, n.value);
    dst.dwFlags.set(OpenMPT::ENV_ENABLED, src.enabled);
    dst.dwFlags.set(OpenMPT::ENV_LOOP,    src.loop);
    dst.dwFlags.set(OpenMPT::ENV_SUSTAIN, src.sustain);
    dst.dwFlags.set(OpenMPT::ENV_CARRY,   src.carry);
    dst.dwFlags.set(OpenMPT::ENV_FILTER,  src.filter);
    dst.nLoopStart    = src.loopStart;
    dst.nLoopEnd      = src.loopEnd;
    dst.nSustainStart = src.sustainStart;
    dst.nSustainEnd   = src.sustainEnd;
}

InstrumentData bb_get_instrument_data(openmpt::module_ext* mod, int instrIndex) {
    InstrumentData d;
    if (!mod) return d;
    auto& sf = getSF(mod);
    if (instrIndex < 1 || instrIndex >= OpenMPT::MAX_INSTRUMENTS) return d;
    auto* ins = sf.Instruments[instrIndex];
    if (!ins) return d;

    // Name / filename
    {
        std::string n = ins->GetName();
        std::string fn = ins->GetFilename();
        strncpy(d.name,     n.c_str(),  31);  d.name[31]     = '\0';
        strncpy(d.filename, fn.c_str(), 31);  d.filename[31] = '\0';
    }

    d.globalVol     = ins->nGlobalVol;
    d.fadeOut       = ins->nFadeOut;
    d.pan           = ins->nPan;
    d.panEnabled    = ins->dwFlags[OpenMPT::INS_SETPANNING];
    d.volSwing      = ins->nVolSwing;
    d.panSwing      = ins->nPanSwing;
    d.pps           = ins->nPPS;
    d.ppc           = ins->nPPC;

    // Filter
    uint8_t ifc = ins->nIFC;
    uint8_t ifr = ins->nIFR;
    d.cutoffEnabled    = (ifc & 0x80) != 0;
    d.cutoff           = ifc & 0x7F;
    d.resonanceEnabled = (ifr & 0x80) != 0;
    d.resonance        = ifr & 0x7F;
    d.cutSwing         = ins->nCutSwing;
    d.resSwing         = ins->nResSwing;
    d.filterMode       = static_cast<uint8_t>(ins->filterMode);

    d.nna        = static_cast<uint8_t>(ins->nNNA);
    d.dct        = static_cast<uint8_t>(ins->nDCT);
    d.dna        = static_cast<uint8_t>(ins->nDNA);
    d.resampling = static_cast<uint8_t>(ins->resampling);

    // Note map and keyboard
    for (int i = 0; i < 128; ++i) {
        d.noteMap[i]  = ins->NoteMap[i];
        d.keyboard[i] = static_cast<uint16_t>(ins->Keyboard[i]);
    }

    copyEnvData(d.volEnv,   ins->VolEnv);
    copyEnvData(d.panEnv,   ins->PanEnv);
    copyEnvData(d.pitchEnv, ins->PitchEnv);

    d.valid = true;
    return d;
}

bool bb_set_instrument_data(openmpt::module_ext* mod, int instrIndex, const InstrumentData& data) {
    if (!mod) return false;
    auto& sf = getSF(mod);
    if (instrIndex < 1 || instrIndex >= OpenMPT::MAX_INSTRUMENTS) return false;
    auto* ins = sf.Instruments[instrIndex];
    if (!ins) return false;

    ins->name     = data.name;
    ins->filename = data.filename;

    ins->nGlobalVol = data.globalVol;
    ins->nFadeOut   = data.fadeOut;
    ins->nPan       = data.pan;
    ins->dwFlags.set(OpenMPT::INS_SETPANNING, data.panEnabled);
    ins->nVolSwing  = data.volSwing;
    ins->nPanSwing  = data.panSwing;
    ins->nPPS       = data.pps;
    ins->nPPC       = data.ppc;

    // Filter (encode enable bits)
    ins->nIFC = data.cutoff    | (data.cutoffEnabled    ? 0x80 : 0);
    ins->nIFR = data.resonance | (data.resonanceEnabled ? 0x80 : 0);
    ins->nCutSwing  = data.cutSwing;
    ins->nResSwing  = data.resSwing;
    ins->filterMode = static_cast<OpenMPT::FilterMode>(data.filterMode);

    ins->nNNA = static_cast<OpenMPT::NewNoteAction>(data.nna);
    ins->nDCT = static_cast<OpenMPT::DuplicateCheckType>(data.dct);
    ins->nDNA = static_cast<OpenMPT::DuplicateNoteAction>(data.dna);
    ins->resampling = static_cast<OpenMPT::ResamplingMode>(data.resampling);

    for (int i = 0; i < 128; ++i) {
        ins->NoteMap[i]  = data.noteMap[i];
        ins->Keyboard[i] = static_cast<OpenMPT::SAMPLEINDEX>(data.keyboard[i]);
    }

    writeEnvData(ins->VolEnv,   data.volEnv);
    writeEnvData(ins->PanEnv,   data.panEnv);
    writeEnvData(ins->PitchEnv, data.pitchEnv);

    return true;
}

int bb_instrument_count(openmpt::module_ext* mod) {
    if (!mod) return 0;
    auto& sf = getSF(mod);
    return static_cast<int>(sf.GetNumInstruments());
}

int bb_sample_count(openmpt::module_ext* mod) {
    if (!mod) return 0;
    auto& sf = getSF(mod);
    return static_cast<int>(sf.GetNumSamples());
}

// ─── Sample helpers ───────────────────────────────────────────────────────────

SampleData bb_get_sample_data(openmpt::module_ext* mod, int sampleIndex) {
    SampleData d;
    if (!mod) return d;
    auto& sf = getSF(mod);
    if (sampleIndex < 1 || sampleIndex > (int)sf.GetNumSamples()) return d;
    const auto& smp = sf.GetSample(static_cast<OpenMPT::SAMPLEINDEX>(sampleIndex));
    const char* n = sf.GetSampleName(sampleIndex);
    strncpy(d.name, n ? n : "", 31); d.name[31] = '\0';
    std::string fn = smp.GetFilename();
    strncpy(d.filename, fn.c_str(), 31); d.filename[31] = '\0';
    d.length       = smp.nLength;
    d.loopStart    = smp.nLoopStart;
    d.loopEnd      = smp.nLoopEnd;
    d.sustainStart = smp.nSustainStart;
    d.sustainEnd   = smp.nSustainEnd;
    d.c5speed      = smp.nC5Speed;
    d.volume       = smp.nVolume;
    d.globalVol    = smp.nGlobalVol;
    d.pan          = smp.nPan;
    d.panEnabled   = smp.uFlags[OpenMPT::CHN_PANNING];
    d.relativeTone = smp.RelativeTone;
    d.fineTune     = smp.nFineTune;
    d.vibType      = static_cast<uint8_t>(smp.nVibType);
    d.vibSweep     = smp.nVibSweep;
    d.vibDepth     = smp.nVibDepth;
    d.vibRate      = smp.nVibRate;
    if (smp.uFlags[OpenMPT::CHN_LOOP])
        d.loopType = smp.uFlags[OpenMPT::CHN_PINGPONGLOOP] ? 2 : 1;
    if (smp.uFlags[OpenMPT::CHN_SUSTAINLOOP])
        d.sustainLoopType = smp.uFlags[OpenMPT::CHN_PINGPONGSUSTAIN] ? 2 : 1;
    d.is16bit   = smp.uFlags[OpenMPT::CHN_16BIT];
    d.isStereo  = smp.uFlags[OpenMPT::CHN_STEREO];
    d.hasData   = smp.HasSampleData();
    d.isOPL     = smp.uFlags[OpenMPT::CHN_ADLIB];
    d.valid     = true;
    return d;
}

bool bb_set_sample_data(openmpt::module_ext* mod, int sampleIndex, const SampleData& d) {
    if (!mod) return false;
    auto& sf = getSF(mod);
    if (sampleIndex < 1 || sampleIndex > (int)sf.GetNumSamples()) return false;
    auto& smp = sf.GetSample(static_cast<OpenMPT::SAMPLEINDEX>(sampleIndex));
    sf.m_szNames[sampleIndex] = d.name;
    smp.filename    = d.filename;
    smp.nC5Speed    = d.c5speed;
    smp.nVolume     = d.volume;
    smp.nGlobalVol  = d.globalVol;
    smp.nPan        = d.pan;
    smp.uFlags.set(OpenMPT::CHN_PANNING, d.panEnabled);
    smp.RelativeTone = d.relativeTone;
    smp.nFineTune   = d.fineTune;
    smp.nVibType    = static_cast<OpenMPT::VibratoType>(d.vibType);
    smp.nVibSweep   = d.vibSweep;
    smp.nVibDepth   = d.vibDepth;
    smp.nVibRate    = d.vibRate;
    smp.nLoopStart    = d.loopStart;
    smp.nLoopEnd      = d.loopEnd;
    smp.nSustainStart = d.sustainStart;
    smp.nSustainEnd   = d.sustainEnd;
    smp.uFlags.set(OpenMPT::CHN_LOOP,            d.loopType > 0);
    smp.uFlags.set(OpenMPT::CHN_PINGPONGLOOP,    d.loopType == 2);
    smp.uFlags.set(OpenMPT::CHN_SUSTAINLOOP,     d.sustainLoopType > 0);
    smp.uFlags.set(OpenMPT::CHN_PINGPONGSUSTAIN, d.sustainLoopType == 2);
    return true;
}

int32_t bb_play_sample(openmpt::module_ext* mod, int sampleIndex, int note, double volume, double panning) {
    if (!mod) return -1;
    auto& sf = getSF(mod);
    if (sampleIndex < 1 || sampleIndex > (int)sf.GetNumSamples()) return -1;
    if (!sf.GetSample(static_cast<OpenMPT::SAMPLEINDEX>(sampleIndex)).HasSampleData()) return -1;
    if (note < OpenMPT::NOTE_MIN || note > OpenMPT::NOTE_MAX) return -1;

    OpenMPT::CHANNELINDEX ch = sf.GetNNAChannel(OpenMPT::CHANNELINDEX_INVALID);
    if (ch == OpenMPT::CHANNELINDEX_INVALID) ch = OpenMPT::MAX_CHANNELS - 1;

    OpenMPT::ModChannel& chn = sf.m_PlayState.Chn[ch];
    chn.Reset(OpenMPT::ModChannel::resetTotal, sf, OpenMPT::CHANNELINDEX_INVALID,
              static_cast<OpenMPT::ChannelFlags>(OpenMPT::CHN_MUTE));
    chn.nMasterChn   = 0;
    chn.nNewNote     = chn.nLastNote = static_cast<uint8_t>(note);
    chn.ResetEnvelopes();
    chn.pModSample      = &sf.GetSample(static_cast<OpenMPT::SAMPLEINDEX>(sampleIndex));
    chn.pModInstrument  = nullptr;
    chn.nFadeOutVol     = 0x10000;
    sf.NoteChange(chn, note, false, true, true);
    if (chn.pModSample->uFlags[OpenMPT::CHN_PANNING])
        chn.nPan = chn.pModSample->nPan;
    else
        chn.nPan = std::max(0, std::min(256, (int)(panning * 128.0 + 128.0)));
    chn.nVolume = std::max(0, std::min(256, (int)(volume  * 256.0)));

    // Remove from mix list so the channel is picked up cleanly next read()
    auto mb = std::begin(sf.m_PlayState.ChnMix);
    auto me = std::remove(mb, mb + sf.m_nMixChannels, ch);
    sf.m_nMixChannels = static_cast<OpenMPT::CHANNELINDEX>(std::distance(mb, me));

    return static_cast<int32_t>(ch);
}

void bb_build_sample_waveform(openmpt::module_ext* mod, int sampleIndex,
                               std::vector<float>& waveMin, std::vector<float>& waveMax,
                               int& outLen, int width) {
    outLen = 0;
    waveMin.clear();
    waveMax.clear();
    if (!mod) return;
    auto& sf = getSF(mod);
    if (sampleIndex < 1 || sampleIndex > (int)sf.GetNumSamples()) return;
    const auto& smp = sf.GetSample(static_cast<OpenMPT::SAMPLEINDEX>(sampleIndex));
    if (!smp.HasSampleData() || smp.nLength == 0) return;

    const bool is16  = smp.uFlags[OpenMPT::CHN_16BIT];
    const int  nCh   = smp.uFlags[OpenMPT::CHN_STEREO] ? 2 : 1;
    const uint32_t len = smp.nLength;
    const int W = std::min(width, (int)len);

    waveMin.resize(W);
    waveMax.resize(W);

    for (int b = 0; b < W; ++b) {
        uint32_t f0 = (uint64_t)b       * len / W;
        uint32_t f1 = (uint64_t)(b + 1) * len / W;
        if (f1 <= f0) f1 = f0 + 1;
        if (f1 > len)  f1 = len;
        float mn = 0.f, mx = 0.f;
        bool first = true;
        for (uint32_t f = f0; f < f1; ++f) {
            float v = is16 ? (smp.pData.pSample16[f * nCh] / 32768.f)
                           : (smp.pData.pSample8 [f * nCh] / 128.f);
            if (first || v < mn) mn = v;
            if (first || v > mx) mx = v;
            first = false;
        }
        waveMin[b] = mn;
        waveMax[b] = mx;
    }
    outLen = W;
}

void bb_setup_channel_capture(openmpt::module_ext* mod, int nChannels) {
    if (!mod || nChannels <= 0) return;
    auto& sf = getSF(mod);
    sf.m_bbNumChannels    = static_cast<OpenMPT::CHANNELINDEX>(
                                std::min(nChannels, (int)OpenMPT::CSoundFile::BB_MAX_CHANNELS));
    sf.m_bbCaptureEnabled = true;
    if (sf.m_opl)
        sf.m_opl->BbSetupCapture(true, nChannels);
}

void bb_set_ft2_volume_ramping(openmpt::module_ext* mod, bool enabled) {
    if (!mod) return;
    getSF(mod).m_playBehaviour.set(OpenMPT::kFT2VolumeRamping, enabled);
}

void bb_capture_channel_waves(openmpt::module_ext* mod,
                               float* samplesOut, int32_t* countsOut,
                               int nChannels, int perChannel) {
    if (!mod || !samplesOut || !countsOut || nChannels <= 0 || perChannel <= 0) return;
    auto& sf = getSF(mod);

    // mixsample_t == float in this build; buffer is interleaved stereo (L R L R …)
    // Apply the same global-volume factor that ProcessGlobalVolume will apply to the
    // master output, so per-channel scale matches what d->left[] contains.
    const float scale = sf.m_bbGlobalVolScale;
    int n = std::min(perChannel, (int)MIXBUFFERSIZE);
    for (int ch = 0; ch < nChannels; ++ch) {
        const OpenMPT::mixsample_t* src = sf.m_bbChannelBuf + ch * MIXBUFFERSIZE * 2;
        float* out = samplesOut + ch * perChannel;
        for (int s = 0; s < n; ++s)
            out[s] = src[s * 2] * scale;   // left channel, scaled to output amplitude
        countsOut[ch] = n;
    }

}

void bb_apply_opl_channel_waves(openmpt::module_ext* mod,
                                 float* chanBuf, int nChannels, int perChannel) {
    if (!mod || !chanBuf || nChannels <= 0 || perChannel <= 0) return;
    auto& sf = getSF(mod);
    if (!sf.m_opl) return;

    OpenMPT::OPL* opl = sf.m_opl.get();
    int nf = std::min(opl->BbGetFrameCount(), perChannel);
    if (nf <= 0) return;

    const float scale = sf.m_bbGlobalVolScale;

    // Write each OPL voice's captured data directly — no ratio normalisation.
    // snap[v] * factor is already in mix-amplitude space; * scale gives output amplitude.
    // This is the voice's isolated contribution regardless of what other voices are doing.
    for (int v = 0; v < OpenMPT::OPL::BB_OPL_VOICES; ++v) {
        OpenMPT::CHANNELINDEX trackerCh = opl->BbGetTrackerChan(v);
        if (trackerCh == OpenMPT::CHANNELINDEX_INVALID || (int)trackerCh >= nChannels)
            continue;
        const float* vframes = opl->BbGetChanFrames(v);
        float* out = chanBuf + (int)trackerCh * perChannel;
        for (int s = 0; s < nf; ++s)
            out[s] += vframes[s] * scale;
    }
}
