// Minimal stubs for OpenMPT symbols not provided by our soundlib build.
#include "common/BuildSettings.h"
#include "common/mptAssert.h"
#include <cstdlib>

OPENMPT_NAMESPACE_BEGIN

MPT_ATTR_NOINLINE MPT_DECL_NOINLINE void AssertHandler(const mpt::source_location &, const char *, const char *) {
    std::abort();
}

OPENMPT_NAMESPACE_END
