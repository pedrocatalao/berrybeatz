#pragma once
#include <functional>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "Transport.h"

struct ImFont;

struct ToolbarState {
    // Playback (App → Toolbar, read-only)
    bool      hasModule  {false};
    bool      isDirty    {false};
    Transport transport  {Transport::Stopped};
    float vuLeft    {0.f};
    float vuRight   {0.f};
    float seekFrac  {0.f};

    // Knob values (read/write — Toolbar updates these, App reads them)
    int   octave    {4};
    int   tempo     {125};
    int   speed     {6};
    int   rpb       {4};
    int   globalVol {128};
    int   gvolMax   {128};

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

    // View toggles
    bool showKeyboard {false};

    // Settings
    std::function<void()>      onSetup;

    // File / transport actions
    std::function<void()>      onNew;
    std::function<void()>      onOpen;
    std::function<void()>      onSave;
    std::function<void()>      onInsertPattern;
    std::function<void()>      onPlayPause;
    std::function<void()>      onRestart;
    std::function<void(float)> onSeek;
    std::function<void(int)>   onOctaveChange;
    std::function<void(int)>   onTempoChange;
    std::function<void(int)>   onSpeedChange;
    std::function<void(int)>   onRPBChange;
    std::function<void(int)>   onGlobalVolChange;
};

float DrawToolbar(ToolbarState& state,
                  ImFont* fontUI, float x, float y, float w, float h);
