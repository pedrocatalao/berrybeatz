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

#include "OpenMPTInternal.h"

#include <fstream>
#include <string>

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
