#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "Theme.h"
#include "Toolbar.h"
#include "LogoView.h"
#include "Knob.h"
#include "SwitchButton.h"
#include "RenderSettings.h"
#include "imgui.h"
#include <algorithm>

static void pushToolbarStyle()
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      IM_COL32(0, 0, 0, 0));  // drawn as gradient below
    ImGui::PushStyleColor(ImGuiCol_Button,        kClrBtnBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kClrBtnHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kClrBtnActive);
    ImGui::PushStyleColor(ImGuiCol_Border,        kClrBtnBorder);
    ImGui::PushStyleColor(ImGuiCol_Text,          kClrTextBright);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,     kToolbarFramePad);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    kStyleFrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      kToolbarItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    kToolbarWindowPad);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
}

static void popToolbarStyle()
{
    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(6);
}

static constexpr ImU32 kVuLoLit    = kVuCyanLit;
static constexpr ImU32 kVuLoUnlit  = kVuCyanUnlit;
static constexpr ImU32 kVuMedLit   = kVuPinkLit;
static constexpr ImU32 kVuMedUnlit = kVuPinkUnlit;
static constexpr ImU32 kVuHiLit    = kVuRedLit;
static constexpr ImU32 kVuHiUnlit  = kVuRedUnlit;

static void drawVU(ImDrawList* dl,
                   float x, float y, float w, float h,
                   float lvlL, float lvlR)
{
    constexpr float barW   = 2.f;
    constexpr float stride = 3.f;
    const float gap  = 2.f;
    const float bh   = (h - gap) * 0.5f;
    const int   N    = std::max(1, (int)((w - 1.f) / stride));
    const float norm[2] = { std::min(lvlL / 1.2f, 1.f), std::min(lvlR / 1.2f, 1.f) };
    const float tops[2] = { y, y + bh + gap };

    if (g_glowVU > 0.f) {
        const float opa = g_glowVU * 0.35f;  // L+R glow accumulates in the overlap
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }, nullptr);
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < N; ++i) {
                float frac = float(i) / float(N);
                if (norm[ch] <= frac) continue;
                int gr, gg, gb;
                if      (frac >= 0.80f) { gr = 200; gg =   0; gb = 255; }
                else if (frac >= 0.60f) { gr = 255; gg =  30; gb = 180; }
                else                    { gr =   0; gg = 220; gb = 255; }
                float sx = x + float(i) * stride;
                float ty = tops[ch];
                dl->AddRectFilled({sx - 16, ty - 6}, {sx + barW + 16, ty + bh + 6}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({sx - 11, ty - 5}, {sx + barW + 11, ty + bh + 5}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({sx -  7, ty - 3}, {sx + barW +  7, ty + bh + 3}, IM_COL32(gr, gg, gb, (uint8_t)( 5 * opa)));
                dl->AddRectFilled({sx -  4, ty - 2}, {sx + barW +  4, ty + bh + 2}, IM_COL32(gr, gg, gb, (uint8_t)( 7 * opa)));
                dl->AddRectFilled({sx -  2, ty - 1}, {sx + barW +  2, ty + bh + 1}, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
                dl->AddRectFilled({sx -  1, ty    }, {sx + barW +  1, ty + bh    }, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
            }
        }
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }, nullptr);
    }

    // Normal bars
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < N; ++i) {
            float frac = float(i) / float(N);
            ImU32 litCol, unlitCol;
            if      (frac >= 0.80f) { litCol = kVuHiLit;  unlitCol = kVuHiUnlit;  }
            else if (frac >= 0.60f) { litCol = kVuMedLit; unlitCol = kVuMedUnlit; }
            else                    { litCol = kVuLoLit;  unlitCol = kVuLoUnlit;  }
            float sx = x + float(i) * stride;
            dl->AddRectFilled({sx, tops[ch]}, {sx + barW, tops[ch] + bh - 1.f},
                              norm[ch] > frac ? litCol : unlitCol);
        }
    }
}

float DrawToolbar(ToolbarState& state,
                  ImFont* fontUI, float x, float y, float w, float h)
{
    pushToolbarStyle();
    ImGui::PushFont(fontUI);   // PushFont(nullptr) falls back to default — always safe

    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);
    ImGui::Begin("##toolbar", nullptr,
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    const float frameH   = ImGui::GetFrameHeight();
    const float knobSz   = 36.f;
    const float btnMidY  = 7.f;
    const float knobMidY = 33.f;

    auto btnY = [&]() { ImGui::SetCursorPosY(btnMidY); };

    // ─── Retro window-control buttons ─────────────────────────────────────────
    // 16×16 squares, drawn manually so they look like old X11 WM buttons.
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        const float bsz = ImGui::GetFrameHeight();
        const float by  = 7.f;  // offset down to clear rounded corner

        int btnID = 0;
        auto wbutton = [&](char sym, ImU32 hoverBg) -> bool {
            ImGui::SetCursorPosY(by);
            ImGui::PushID(btnID++);
            ImVec2 p = ImGui::GetCursorScreenPos();
            bool clicked = ImGui::InvisibleButton("##wc", {bsz, bsz});
            bool hov = ImGui::IsItemHovered();
            dl->AddRectFilled(p, {p.x+bsz, p.y+bsz},
                              hov ? hoverBg : IM_COL32(38, 38, 58, 255));
            float m = 4.f;
            ImU32 sc = IM_COL32(210, 210, 220, 255);
            if (sym == 'x') {
                dl->AddLine({p.x+m,      p.y+m},      {p.x+bsz-m, p.y+bsz-m}, sc, 1.5f);
                dl->AddLine({p.x+bsz-m,  p.y+m},      {p.x+m,     p.y+bsz-m}, sc, 1.5f);
            } else if (sym == '-') {
                float my = p.y + bsz * 0.5f;
                dl->AddLine({p.x+m, my}, {p.x+bsz-m, my}, sc, 1.5f);
            } else if (sym == '+') {
                dl->AddRect({p.x+m, p.y+m}, {p.x+bsz-m, p.y+bsz-m}, sc, 0.f, 0, 1.5f);
            } else if (sym == 'f') {
                // Four outward-pointing L-corners — classic fullscreen icon
                const float ci = m - 0.5f;  // inset from button edge
                const float cl = 3.5f;      // arm length
                // top-left
                dl->AddLine({p.x+ci,      p.y+ci},      {p.x+ci+cl,  p.y+ci},      sc, 1.3f);
                dl->AddLine({p.x+ci,      p.y+ci},      {p.x+ci,     p.y+ci+cl},   sc, 1.3f);
                // top-right
                dl->AddLine({p.x+bsz-ci,  p.y+ci},      {p.x+bsz-ci-cl, p.y+ci},  sc, 1.3f);
                dl->AddLine({p.x+bsz-ci,  p.y+ci},      {p.x+bsz-ci, p.y+ci+cl},  sc, 1.3f);
                // bottom-left
                dl->AddLine({p.x+ci,      p.y+bsz-ci},  {p.x+ci+cl,  p.y+bsz-ci}, sc, 1.3f);
                dl->AddLine({p.x+ci,      p.y+bsz-ci},  {p.x+ci,     p.y+bsz-ci-cl}, sc, 1.3f);
                // bottom-right
                dl->AddLine({p.x+bsz-ci,  p.y+bsz-ci},  {p.x+bsz-ci-cl, p.y+bsz-ci}, sc, 1.3f);
                dl->AddLine({p.x+bsz-ci,  p.y+bsz-ci},  {p.x+bsz-ci, p.y+bsz-ci-cl}, sc, 1.3f);
            }
            ImGui::PopID();
            return clicked;
        };

        ImGui::SetCursorPosX(8.f);
        if (wbutton('x', IM_COL32(140, 28, 28, 255)))
            { if (state.onWindowClose)    state.onWindowClose();    }
        ImGui::SameLine(0.f, 3.f);
        if (wbutton('-', IM_COL32(120, 95, 20, 255)))
            { if (state.onWindowMinimize) state.onWindowMinimize(); }
        ImGui::SameLine(0.f, 3.f);
        if (wbutton('+', IM_COL32(22, 68, 120, 255)))
            { if (state.onWindowMaximize)    state.onWindowMaximize();    }
        ImGui::SameLine(0.f, 3.f);
        if (wbutton('f', IM_COL32(18, 110, 75, 255)))
            { if (state.onWindowFullscreen)  state.onWindowFullscreen();  }

        ImGui::SameLine(0.f, 8.f);
        ImGui::SetCursorPosY(by);
        if (SwitchButton("New##new",    "New"))    { if (state.onNew)   state.onNew();   }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        if (SwitchButton("Open##open",  "Open"))   { if (state.onOpen)  state.onOpen();  }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        if (SwitchButton("Save##save",  "Save"))   { if (state.onSave)  state.onSave();  }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        if (SwitchButton("Config##cfg", "Config")) { if (state.onSetup) state.onSetup(); }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        // Window drag is handled in the GLFW mouse button callback (main.cpp),
        // which fires from inside GLFW's mouseDown: delegate — the only context
        // where performWindowDragWithEvent: is valid.
    }

    ImGui::SameLine(0.f, 10.f);

    // ─── Transport ────────────────────────────────────────────────────────────
    if (!state.hasModule) ImGui::BeginDisabled();

    // Stop button — red LED when stopped
    btnY();
    const bool isStopped = state.transport == Transport::Stopped;
    if (SwitchButton("Stop##stop", "Stop", (int)isStopped, 1, {0.f, 0.f}, IM_COL32(220, 60, 60, 255)))
        { if (state.onStop) state.onStop(); }

    // Play/Pause — fixed width (sized for "Pause"), green LED playing, blue LED paused
    ImGui::SameLine();
    btnY();
    const bool isPlaying = state.transport == Transport::Playing;
    const bool isPaused  = state.transport == Transport::Paused;
    const char* playLbl  = isPlaying ? "Pause" : "Play";
    const float playBtnW = ImGui::CalcTextSize("Pause").x
                         + ImGui::GetStyle().FramePadding.x + 16.f; // PAD_X + LED area
    ImU32 ppLedCol = isPlaying ? IM_COL32(80, 200, 100, 255) : IM_COL32(80, 120, 255, 255);
    if (SwitchButton("PlayPause##pp", playLbl, (int)(isPlaying || isPaused), 1, {playBtnW, 0.f}, ppLedCol))
        { if (state.onPlayPause) state.onPlayPause(); }

    if (!state.hasModule) ImGui::EndDisabled();

    ImGui::SameLine(0.f, 10.f);

    // ─── Seek slider ──────────────────────────────────────────────────────────
    if (!state.hasModule) ImGui::BeginDisabled();

    ImGui::SetNextItemWidth(174.f);
    btnY();
    float seek = state.seekFrac;
    if (ImGui::SliderFloat("##seek", &seek, 0.f, 1.f, "")) {
        if (state.onSeek) state.onSeek(seek);
    }

    if (!state.hasModule) ImGui::EndDisabled();

    ImGui::SetCursorPos({13.f, knobMidY});

    // ─── Knobs ────────────────────────────────────────────────────────────────
    ImDrawList* dl = ImGui::GetWindowDrawList();

    auto doKnob = [&](const char* id, int* val, int vmin, int vmax,
                      std::function<void(int)> cb)
    {
        ImGui::SetCursorPosY(knobMidY);
        if (KnobEdit(id, val, vmin, vmax, false, id, KnobLabelPos::Left) && cb)
            cb(*val);
    };

    doKnob("Oct", &state.octave,    0,   8,   state.onOctaveChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("BPM", &state.tempo,     32,  512, state.onTempoChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("Spd", &state.speed,     1,   255, state.onSpeedChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("RPB", &state.rpb,       1,   32,  state.onRPBChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("Vol", &state.globalVol, 0,   state.gvolMax, state.onGlobalVolChange);

    // ─── VU meters (knob row) ─────────────────────────────────────────────────
    {
        const float vuW   = 202.f;
        const float vuH   = 25.f;
        const float kGap  = 4.f;
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        const float lblW  = ImGui::CalcTextSize("VU").x;
        const float lblH  = ImGui::GetTextLineHeight();
        if (g_fontKnobLabel) ImGui::PopFont();
        float vuX = ImGui::GetItemRectMax().x + 12.f + lblW + kGap;
        float vuY = ImGui::GetWindowPos().y + knobMidY + (knobSz - vuH) * 0.5f;
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        dl->AddText({vuX - kGap - lblW, vuY + (vuH - lblH) * 0.5f}, kClrTextLabel, "VU");
        if (g_fontKnobLabel) ImGui::PopFont();
        drawVU(dl, vuX, vuY, vuW, vuH, state.vuLeft, state.vuRight);
        ImGui::SetCursorScreenPos({vuX - lblW - kGap, ImGui::GetWindowPos().y + knobMidY});
        ImGui::Dummy({vuW + lblW + kGap, knobSz});
    }

    // ─── Logo (top-right) ─────────────────────────────────────────────────────
    {
        static LogoView logo;
        ImVec2 winPos = ImGui::GetWindowPos();
        logo.draw(winPos.x, winPos.y, ImGui::GetWindowSize().x,
                  state.logoTex, state.logoW, state.logoH,
                  state.speakerConeTex, state.speakerConeW, state.speakerConeH,
                  state.vuLeft, state.vuRight);
    }

    ImGui::End();
    ImGui::PopFont();
    popToolbarStyle();

    return y + h;
}
