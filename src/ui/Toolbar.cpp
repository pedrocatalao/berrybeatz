#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "Theme.h"
#include "Toolbar.h"
#include "Knob.h"
#include "RenderSettings.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

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

    // ─── Gradient background (black → dark grey, top to bottom) ──────────────
    {
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
            wp, {wp.x + ws.x, wp.y + ws.y},
            IM_COL32(  0,   0,  20, 255),   // top-left
            IM_COL32(  0,   0,  20, 255),   // top-right
            IM_COL32( 20,  20,  40, 255),   // bottom-right
            IM_COL32( 20,  20,  40, 255));  // bottom-left
    }

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
        if (ImGui::Button("New"))  { if (state.onNew)  state.onNew();  }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        if (ImGui::Button("Open")) { if (state.onOpen) state.onOpen(); }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        if (ImGui::Button("Save")) { if (state.onSave) state.onSave(); }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        if (ImGui::Button("Config"))  { if (state.onSetup) state.onSetup(); }

        // Window drag is handled in the GLFW mouse button callback (main.cpp),
        // which fires from inside GLFW's mouseDown: delegate — the only context
        // where performWindowDragWithEvent: is valid.
    }

    ImGui::SameLine(0.f, 10.f);

    // ─── Transport ────────────────────────────────────────────────────────────
    if (!state.hasModule) ImGui::BeginDisabled();

    btnY();
    if (ImGui::Button("Stop")) { if (state.onStop) state.onStop(); }
    ImGui::SameLine();
    btnY();
    const char* playLbl = (state.isPlaying && !state.isPaused) ? "Pause" : "Play";
    const float playBtnW = ImGui::CalcTextSize("Pause").x + ImGui::GetStyle().FramePadding.x * 2.f;
    if (ImGui::Button(playLbl, {playBtnW, 0.f})) { if (state.onPlayPause) state.onPlayPause(); }
    ImGui::SameLine();
    btnY();
    if (ImGui::Button("Restart")) { if (state.onPlayFromStart) state.onPlayFromStart(); }

    if (!state.hasModule) ImGui::EndDisabled();

    ImGui::SameLine(0.f, 10.f);

    // ─── Seek slider ──────────────────────────────────────────────────────────
    if (!state.hasModule) ImGui::BeginDisabled();

    ImGui::SetNextItemWidth(165.f);
    btnY();
    float seek = state.seekFrac;
    if (ImGui::SliderFloat("##seek", &seek, 0.f, 1.f, "")) {
        if (state.onSeek) state.onSeek(seek);
    }

    if (!state.hasModule) ImGui::EndDisabled();

    ImGui::SetCursorPos({13.f, knobMidY});

    // ─── Knobs ────────────────────────────────────────────────────────────────
    // Each knob is 36×36; draw label text below via the draw list.
    // Clicking without dragging activates an in-place InputInt overlay.
    static const char* s_editLabel = nullptr;
    static int         s_editVal   = 0;
    static bool        s_doFocus   = false;
    static float       s_editX     = 0.f;   // screen x where the value text starts
    static float       s_editY     = 0.f;   // screen y for the text baseline

    auto doKnob = [&](const char* label, int* val, int vmin, int vmax,
                      std::function<void(int)> cb)
    {
        ImGui::SetCursorPosY(knobMidY);

        // Label to the left of the knob, vertically centred
        const float labelGap = 5.f;
        ImVec2 labelSp = ImGui::GetCursorScreenPos();
        ImVec2 textSz  = ImGui::CalcTextSize(label);
        ImGui::GetWindowDrawList()->AddText(
            {labelSp.x, labelSp.y + (knobSz - textSz.y) * 0.5f},
            IM_COL32(160, 160, 180, 255), label);

        // Advance cursor past the label so the knob sits to its right
        ImGui::SetCursorScreenPos({labelSp.x + textSz.x + labelGap, labelSp.y});
        ImVec2 sp = ImGui::GetCursorScreenPos();

        bool editing = (s_editLabel == label);

        bool editReq = false;
        bool changed = KnobWidget(label, val, vmin, vmax, &editReq, editing);
        if (changed && cb) cb(*val);

        if (editReq) {
            s_editLabel = label;
            s_editVal   = *val;
            s_doFocus   = true;
            char ibuf[16];
            snprintf(ibuf, sizeof(ibuf), "%d", *val);
            ImVec2 vsz = ImGui::CalcTextSize(ibuf);
            s_editX = sp.x + (knobSz - vsz.x) * 0.5f;
            s_editY = sp.y + (knobSz - vsz.y) * 0.5f;
        }

        // In-place number edit: transparent, borderless InputInt drawn exactly
        // where the value text lives; a Dummy at the knob footprint resets the
        // ImGui last-item rect so SameLine on the caller side stays correct.
        if (editing) {
            char ibuf[16];
            snprintf(ibuf, sizeof(ibuf), "%d", s_editVal);
            const float iw = ImGui::CalcTextSize(ibuf).x + ImGui::GetTextLineHeight();

            ImGui::SetCursorScreenPos({s_editX, s_editY});
            ImGui::SetNextItemWidth(iw);
            if (s_doFocus) { ImGui::SetKeyboardFocusHere(); s_doFocus = false; }

            ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text,           IM_COL32(208, 208, 80, 255));
            ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_NavHighlight,   IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));

            ImGui::PushID(label);
            if (ImGui::InputInt("##ke", &s_editVal, 0, 0,
                    ImGuiInputTextFlags_EnterReturnsTrue |
                    ImGuiInputTextFlags_AutoSelectAll)) {
                s_editVal = s_editVal < vmin ? vmin : (s_editVal > vmax ? vmax : s_editVal);
                *val = s_editVal;
                if (cb) cb(*val);
                s_editLabel = nullptr;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                s_editLabel = nullptr;
            ImGui::PopID();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(6);

            // Dummy at the knob footprint: resets last-item rect so SameLine
            // after this doKnob call places the next knob at the right position.
            ImGui::SetCursorScreenPos(sp);
            ImGui::Dummy({knobSz, knobSz});
        }
    };

    const float kPad  = 5.f;
    const float rowX0 = ImGui::GetCursorScreenPos().x - kPad;
    const float rowY0 = ImGui::GetWindowPos().y + knobMidY - kPad;

    // Split channels: knobs/VU draw in ch1 (front), background fills in ch0 (back)
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    doKnob("Oct",  &state.octave,    0,   8,   state.onOctaveChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("BPM",  &state.tempo,     32,  512, state.onTempoChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("Spd",  &state.speed,     1,   255, state.onSpeedChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("RPB",  &state.rpb,       1,   32,  state.onRPBChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("Vol", &state.globalVol, 0,   256, state.onGlobalVolChange);

    // ─── VU meters (knob row) ─────────────────────────────────────────────────
    {
        const float vuW = 202.f;
        const float vuH = 25.f;
        float vuX = ImGui::GetItemRectMax().x + 12.f;
        float vuY = ImGui::GetWindowPos().y + knobMidY + (knobSz - vuH) * 0.5f;
        drawVU(dl, vuX, vuY, vuW, vuH, state.vuLeft, state.vuRight);
        ImGui::SetCursorScreenPos({vuX, ImGui::GetWindowPos().y + knobMidY});
        ImGui::Dummy({vuW, knobSz});
    }

    // ─── Background fill behind knobs + VU ───────────────────────────────────
    {
        float rowX1 = ImGui::GetItemRectMax().x + kPad;
        float rowY1 = ImGui::GetWindowPos().y + knobMidY + knobSz + kPad;
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled({rowX0, rowY0}, {rowX1, rowY1},
                          IM_COL32(28, 28, 44, 86));
        dl->ChannelsMerge();
    }

    // ─── Logo (top-right) ─────────────────────────────────────────────────────
    if (state.logoTex && state.logoH > 0.f) {
        const float lh  = 90.f;
        const float lw  = lh * (state.logoW / state.logoH);
        ImVec2 winPos   = ImGui::GetWindowPos();
        float  winW     = ImGui::GetWindowSize().x;
        ImVec2 p0       = {winPos.x + winW - lw - 6.f, winPos.y + 6.f};

        ImDrawList* fdl = ImGui::GetForegroundDrawList();

        auto drawQuad = [&](float ox, float oy, ImU32 col) {
            fdl->AddImageQuad(
                (ImTextureID)(intptr_t)state.logoTex,
                {p0.x + ox,      p0.y + oy            },
                {p0.x + lw + ox, p0.y + oy      },
                {p0.x + lw + ox, p0.y + lh + oy },
                {p0.x + ox,      p0.y + lh + oy        },
                {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f},
                col);
        };

        // Glow passes use additive blending so they add light to the background
        // rather than accumulating alpha over the main image.
        fdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }, nullptr);

        if (g_glowLogo > 0.f) {
            auto ga = [](int a) { return (uint8_t)(a * g_glowLogo); };
            static const float kDirs[8][2] = {
                {-1.f, 0.f},{1.f, 0.f},{0.f,-1.f},{0.f, 1.f},
                {-1.f,-1.f},{1.f,-1.f},{-1.f,1.f},{1.f, 1.f}
            };
            for (auto& d : kDirs) drawQuad(d[0]*5.f,  d[1]*5.f,  IM_COL32(160, 160, 255, ga( 8)));
            for (auto& d : kDirs) drawQuad(d[0]*3.f,  d[1]*3.f,  IM_COL32(180, 180, 255, ga(13)));
            for (auto& d : kDirs) drawQuad(d[0]*1.5f, d[1]*1.5f, IM_COL32(200, 200, 255, ga(18)));
        }

        // Restore normal blending before drawing the main image
        fdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }, nullptr);

        drawQuad(0.f, 0.f, IM_COL32(255, 255, 255, 240));

        // ── Speaker-cone overlay ──────────────────────────────────────────────
        // Position and size in screen pixels, relative to the logo rect's top-left (p0).
        // Adjust kConeX / kConeY to move; kConeScale to resize.
        static constexpr float kConeX     = 231.f;  // screen px from p0.x
        static constexpr float kConeY     =  38.f;  // screen px from p0.y
        static constexpr float kConeScale =   1.0f; // size multiplier

        if (state.speakerConeTex) {
            const float coneW = state.speakerConeW * (lh / 463.f) * kConeScale;
            const float coneH = state.speakerConeH * (lh / 463.f) * kConeScale;

            const float vuPeak = std::min((state.vuLeft + state.vuRight) * 0.5f, 1.0f);
            const float vibX   = std::sin((float)ImGui::GetTime() * 2.f * 3.14159265f * 28.f) * vuPeak * 2.f;

            ImVec2 cTL = {p0.x + kConeX + vibX,        p0.y + kConeY        };
            ImVec2 cTR = {p0.x + kConeX + coneW + vibX, p0.y + kConeY        };
            ImVec2 cBR = {p0.x + kConeX + coneW + vibX, p0.y + kConeY + coneH};
            ImVec2 cBL = {p0.x + kConeX + vibX,        p0.y + kConeY + coneH};

            fdl->AddImageQuad(
                (ImTextureID)(intptr_t)state.speakerConeTex,
                cTL, cTR, cBR, cBL,
                {0.f,0.f}, {1.f,0.f}, {1.f,1.f}, {0.f,1.f},
                IM_COL32(255, 255, 255, 255));
        }
    }

    ImGui::End();
    ImGui::PopFont();
    popToolbarStyle();

    return y + h;
}
