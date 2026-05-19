#include "Toolbar.h"
#include "Knob.h"
#include "imgui.h"
#include <algorithm>

static void pushToolbarStyle()
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      IM_COL32( 30,  30,  46, 255));
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

static void drawVU(ImDrawList* dl,
                   float x, float y, float w, float h,
                   float lvlL, float lvlR)
{
    const float gap = 2.f;
    const float bw  = (w - gap) * 0.5f;

    auto bar = [&](float bx, float level) {
        dl->AddRectFilled({bx, y}, {bx + bw, y + h}, IM_COL32(20, 20, 36, 255));
        float filled = std::min(level, 1.f) * h;
        if (filled > 0.f)
            dl->AddRectFilled({bx, y + h - filled}, {bx + bw, y + h},
                              IM_COL32(60, 200, 60, 255));
        dl->AddRect({bx, y}, {bx + bw, y + h}, IM_COL32(64, 64, 90, 255));
    };

    bar(x,           lvlL);
    bar(x + bw + gap, lvlR);
}

float DrawToolbar(ToolbarState& state, KnobEditRequest& knobEdit,
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
    const float btnMidY  = (h - frameH) * 0.5f;
    const float knobMidY = (h - knobSz) * 0.5f;

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

        // Window drag is handled in the GLFW mouse button callback (main.cpp),
        // which fires from inside GLFW's mouseDown: delegate — the only context
        // where performWindowDragWithEvent: is valid.
    }

    ImGui::SameLine(0.f, 10.f);

    // ─── File buttons ─────────────────────────────────────────────────────────
    btnY();
    if (ImGui::Button("New"))  { if (state.onNew)  state.onNew();  }
    ImGui::SameLine();
    btnY();
    if (ImGui::Button("Open")) { if (state.onOpen) state.onOpen(); }
    ImGui::SameLine();
    btnY();
    if (ImGui::Button("Save")) { if (state.onSave) state.onSave(); }

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

    // ─── Knobs ────────────────────────────────────────────────────────────────
    // Each knob is 36×36; draw label text below via the draw list.
    auto doKnob = [&](const char* label, int* val, int vmin, int vmax,
                      std::function<void(int)> cb)
    {
        ImGui::SetCursorPosY(knobMidY);
        ImVec2 sp  = ImGui::GetCursorScreenPos();
        bool editReq = false;
        bool changed = KnobWidget(label, val, vmin, vmax, &editReq);
        if (changed && cb) cb(*val);

        if (editReq && !knobEdit.pending) {
            knobEdit.pending   = true;
            knobEdit.label     = label;
            knobEdit.value     = *val;
            knobEdit.vmin      = vmin;
            knobEdit.vmax      = vmax;
            knobEdit.popupX    = sp.x;
            knobEdit.popupY    = sp.y + knobSz + 4.f;
            knobEdit.onConfirm = [val, cb](int v) {
                *val = v;
                if (cb) cb(v);
            };
        }

        // Label below the knob
        ImVec2 textSz = ImGui::CalcTextSize(label);
        ImGui::GetWindowDrawList()->AddText(
            {sp.x + (knobSz - textSz.x) * 0.5f, sp.y + knobSz + 2.f},
            IM_COL32(160, 160, 180, 255), label);
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

    ImGui::SameLine(0.f, 12.f);

    // ─── VU meters ────────────────────────────────────────────────────────────
    {
        const float vuW = 20.f;
        const float vuH = h - 8.f;
        ImVec2 sp = ImGui::GetCursorScreenPos();
        drawVU(ImGui::GetWindowDrawList(),
               sp.x, y + 4.f, vuW, vuH,
               state.vuLeft, state.vuRight);
        ImGui::Dummy({vuW, vuH});
    }

    ImGui::End();
    ImGui::PopFont();
    popToolbarStyle();

    return y + h;
}
