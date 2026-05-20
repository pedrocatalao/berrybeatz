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
