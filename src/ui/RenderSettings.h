#pragma once

// Shared rendering settings — written by App on settings apply,
// read by all UI rendering code on the same thread.
inline float g_glowLevel   = 1.0f;  // 0.0 = off, 1.0 = default, 2.0 = max
inline bool  g_glowLogo    = true;
inline bool  g_glowVU      = true;
inline bool  g_glowPattern = true;
inline bool  g_glowKnobs   = true;
