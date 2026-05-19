#include "Toolbar.h"
#include "Knob.h"
#include "imgui.h"
#include <algorithm>

static void pushToolbarStyle()
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      IM_COL32(  0,   0,   0,   0));  // drawn as gradient below
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32( 45,  45,  68, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32( 65,  65,  98, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32( 80,  80, 120, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32( 80,  80, 112, 255));
    ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(208, 208, 224, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,     ImVec2(8.f, 2.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(3.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(8.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
}

static void popToolbarStyle()
{
    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(6);
}

static const ImU32 kVuLoLit    = IM_COL32(  0, 200,   0, 255);
static const ImU32 kVuLoUnlit  = IM_COL32(  0,  28,   0, 255);
static const ImU32 kVuMedLit   = IM_COL32(255, 200,   0, 255);
static const ImU32 kVuMedUnlit = IM_COL32( 40,  30,   0, 255);
static const ImU32 kVuHiLit    = IM_COL32(225,   0,   0, 255);
static const ImU32 kVuHiUnlit  = IM_COL32( 35,   0,   0, 255);

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
    const float btnMidY  = 5.f;
    const float knobMidY = 26.f;

    auto btnY = [&]() { ImGui::SetCursorPosY(btnMidY); };

    // ─── Retro window-control buttons ─────────────────────────────────────────
    // 16×16 squares, drawn manually so they look like old X11 WM buttons.
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        const float bsz = 16.f;
        const float by  = 5.f;  // near top, not centred

        int btnID = 0;
        auto wbutton = [&](char sym, ImU32 hoverBg) -> bool {
            ImGui::SetCursorPosY(by);
            ImGui::PushID(btnID++);
            ImVec2 p = ImGui::GetCursorScreenPos();
            bool clicked = ImGui::InvisibleButton("##wc", {bsz, bsz});
            bool hov = ImGui::IsItemHovered();
            dl->AddRectFilled(p, {p.x+bsz, p.y+bsz},
                              hov ? hoverBg : IM_COL32(38, 38, 58, 255));
            dl->AddRect(p, {p.x+bsz, p.y+bsz}, IM_COL32(72, 72, 100, 255));
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
    if (ImGui::Button(playLbl)) { if (state.onPlayPause) state.onPlayPause(); }
    ImGui::SameLine();
    btnY();
    if (ImGui::Button("|<")) { if (state.onPlayFromStart) state.onPlayFromStart(); }

    if (!state.hasModule) ImGui::EndDisabled();

    ImGui::SameLine(0.f, 10.f);

    // ─── Seek slider ──────────────────────────────────────────────────────────
    if (!state.hasModule) ImGui::BeginDisabled();

    ImGui::SetNextItemWidth(160.f);
    btnY();
    float seek = state.seekFrac;
    if (ImGui::SliderFloat("##seek", &seek, 0.f, 1.f, "")) {
        if (state.onSeek) state.onSeek(seek);
    }

    if (!state.hasModule) ImGui::EndDisabled();

    ImGui::SameLine(0.f, 12.f);

    // ─── VU meters ────────────────────────────────────────────────────────────
    {
        const float vuW = 120.f;
        const float vuH = 18.f;
        ImVec2 sp = ImGui::GetCursorScreenPos();
        drawVU(ImGui::GetWindowDrawList(),
               sp.x, y + 5.f, vuW, vuH,
               state.vuLeft, state.vuRight);
        ImGui::Dummy({vuW, vuH});
    }

    ImGui::SetCursorPos({0.f, 26.f});

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

    doKnob("Oct",  &state.octave,    0,   8,   state.onOctaveChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("BPM",  &state.tempo,     32,  512, state.onTempoChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("Spd",  &state.speed,     1,   255, state.onSpeedChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("RPB",  &state.rpb,       1,   32,  state.onRPBChange);
    ImGui::SameLine(0.f, 6.f);
    doKnob("GVol", &state.globalVol, 0,   256, state.onGlobalVolChange);

    // ─── Logo (top-right) ─────────────────────────────────────────────────────
    if (state.logoTex && state.logoH > 0.f) {
        const float lh  = 79.f;
        const float lw  = lh * (state.logoW / state.logoH);
        ImVec2 winPos   = ImGui::GetWindowPos();
        float  winW     = ImGui::GetWindowSize().x;
        ImVec2 p0       = {winPos.x + winW - lw - 6.f, winPos.y + 6.f};

        // Slight tilt: top-right corner lifted 5px
        ImGui::GetForegroundDrawList()->AddImageQuad(
            (ImTextureID)(intptr_t)state.logoTex,
            {p0.x,      p0.y     },   // top-left     (normal)
            {p0.x + lw, p0.y - 4.f}, // top-right    (5px up)
            {p0.x + lw, p0.y + lh + 2.f},  // bottom-right (normal)
            {p0.x,      p0.y + lh},  // bottom-left  (normal)
            {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f},
            IM_COL32(255, 255, 255, 180));
    }

    ImGui::End();
    ImGui::PopFont();
    popToolbarStyle();

    return y + h;
}
