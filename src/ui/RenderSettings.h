#pragma once

// Shared rendering settings — written by App on settings apply,
// read by all UI rendering code on the same thread.
inline float g_glowLogo    = 1.0f;  // 0.0 = off, 1.0 = default (slider 5), 2.0 = max (slider 10)
inline float g_glowVU      = 1.0f;
inline float g_glowPattern = 1.0f;
inline float g_glowKnobs   = 1.0f;
inline int   g_cursorReq   = 0;  // 0=arrow 1=openHand 2=closedHand; widgets write, App applies once per frame
