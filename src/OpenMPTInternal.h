#pragma once
#include <string>

namespace openmpt { class module_ext; }

// Save module to path in its original format.
// 'type' is the string returned by module->get_metadata("type"), e.g. "it", "xm", "s3m", "mod".
// Returns true on success.
bool bb_save_module(openmpt::module_ext* mod, const std::string& path, const std::string& type);

// Insert a new empty pattern with the given row count at the lowest available index.
// Returns the new pattern index, or -1 on failure.
int bb_insert_pattern(openmpt::module_ext* mod, int rows = 64);
