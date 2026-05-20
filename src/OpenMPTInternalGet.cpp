// This TU extracts CSoundFile* from a module_ext via libopenmpt internal headers.
// Must NOT include soundlib headers beyond what libopenmpt_ext_impl.hpp pulls in.
// LIBOPENMPT_BUILD is set via compile definitions (CMakeLists).
// LIBOPENMPT_BUILD_TEST enables MPT_ENABLE_CHARSET_LOCALE (needed for libopenmpt_impl.hpp
// line 49: using PathString = mpt::native_path, inside OpenMPT::mpt namespace).
#define LIBOPENMPT_BUILD_TEST

// stdafx.h establishes ::mpt with full type definitions (native_path, BasicPathString,
// NativePathTraits, etc.) before libopenmpt_impl.hpp forward-declares them inside
// ::mpt::mpt_libopenmpt — both refer to the same inline namespace, so the definitions
// are already present when the fwd-decls are encountered.
#include "common/stdafx.h"

#include "libopenmpt/libopenmpt_ext_impl.hpp"

void* bb_get_soundfile_ptr(openmpt::module_ext* mod) {
    // module_ext stores a private module_ext_impl* ext_impl.
    // module_ext constructor calls set_impl(ext_impl) so module::impl == ext_impl.
    // module_impl::GetSoundFile() is inline and returns *m_sndFile.
    // We read module::impl through the known struct layout (vptr, then impl pointer).
    struct ModuleLayout {
        void*                 vptr;
        openmpt::module_impl* impl;
    };
    auto* layout = reinterpret_cast<ModuleLayout*>(mod);
    // module::impl points to a module_ext_impl (set via set_impl in the constructor).
    // Cast to module_ext_impl* is valid since that IS the dynamic type.
    auto* extImpl = static_cast<openmpt::module_ext_impl*>(layout->impl);
    return &extImpl->GetSoundFile();
}
