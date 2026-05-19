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
    float seekFrac  {0.f};

    // Knob values (read/write — Toolbar updates these, App reads them)
    int   octave    {4};
    int   tempo     {125};
    int   speed     {6};
    int   rpb       {4};
    int   globalVol {128};

    // Window controls (Toolbar → App)
    std::function<void()>           onWindowClose;
    std::function<void()>           onWindowMinimize;
    std::function<void()>           onWindowMaximize;
    std::function<void()>            onWindowFullscreen;
    // (window drag is handled at GLFW callback level, not through ToolbarState)

    // File / transport actions
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

float DrawToolbar(ToolbarState& state, KnobEditRequest& knobEdit,
                  ImFont* fontUI, float x, float y, float w, float h);
