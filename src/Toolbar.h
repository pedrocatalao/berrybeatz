#pragma once
#include <functional>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>

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

    // Logo rendered at top-right (0 = no logo)
    GLuint logoTex {0};
    float  logoW   {0.f};
    float  logoH   {0.f};

    // Speaker cone overlay (rendered on top of the logo's cone position)
    GLuint speakerConeTex {0};
    float  speakerConeW   {0.f};
    float  speakerConeH   {0.f};

    // File / transport actions
    std::function<void()>      onNew;
    std::function<void()>      onOpen;
    std::function<void()>      onSave;
    std::function<void()>      onInsertPattern;
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

float DrawToolbar(ToolbarState& state,
                  ImFont* fontUI, float x, float y, float w, float h);
