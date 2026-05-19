#pragma once
#include <functional>
#include <string>

struct ImFont;

struct ToolbarState {
    // Playback (App → Toolbar, read-only)
    bool  hasModule {false};
    bool  isPlaying {false};
    bool  isPaused  {false};
    float vuLeft    {0.f};
    float vuRight   {0.f};
    float seekFrac  {0.f};   // 0..1 normalized position

    // Knob values (read/write — Toolbar updates these, App reads them)
    int   octave    {4};
    int   tempo     {125};
    int   speed     {6};
    int   rpb       {4};
    int   globalVol {128};

    // Actions (Toolbar → App)
    std::function<void()>      onNew;
    std::function<void()>      onOpen;
    std::function<void()>      onSave;
    std::function<void()>      onStop;
    std::function<void()>      onPlayPause;
    std::function<void()>      onPlayFromStart;
    std::function<void(float)> onSeek;
    std::function<void(int)>   onOctaveChange;
    std::function<void(int)>   onTempoChange;
    std::function<void(int)>   onSpeedChange;
    std::function<void(int)>   onRPBChange;
    std::function<void(int)>   onGlobalVolChange;
};

// Knob edit request — set by DrawToolbar when user clicks a knob without dragging.
// Caller (App) should open an ImGui popup to let the user type a value.
struct KnobEditRequest {
    bool                     pending  {false};
    std::string              label;
    int                      value    {0};
    int                      vmin     {0};
    int                      vmax     {100};
    float                    popupX   {0.f};
    float                    popupY   {0.f};
    std::function<void(int)> onConfirm;
};

// Draw the main toolbar as a fixed ImGui window at (x, y) with size (w × h).
// Returns the bottom Y of the toolbar (y + h).
float DrawToolbar(ToolbarState& state, KnobEditRequest& knobEdit,
                  ImFont* fontUI, float x, float y, float w, float h);
