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
    const int   N    = std::max(1, (int)((w - barW - 1) / stride) + 1);
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

static void drawVUWidget(ImDrawList* dl, ImVec2 p, float boxW, float boxH,
                         float lvlL, float lvlR)
{
    constexpr float ROUNDING = 1.5f;
    constexpr float PAD_X    = 5.f;
    constexpr float PAD_Y    = 3.f;
    constexpr float LBL_GAP  = 4.f;
    constexpr float LCD_R    = 2.f;
    constexpr float LCD_PAD  = 2.f;

    const ImVec2 q = {p.x + boxW, p.y + boxH};

    // Outer button box (SwitchButton style, display-only)
    dl->AddRectFilledMultiColor(p, q,
        IM_COL32(33, 33, 48, 255), IM_COL32(33, 33, 48, 255),
        IM_COL32(14, 14, 26, 255), IM_COL32(14, 14, 26, 255));
    dl->AddLine({p.x + ROUNDING, p.y + 0.5f}, {q.x - ROUNDING, p.y + 0.5f}, IM_COL32(255,255,255,55));
    dl->AddLine({p.x + 0.5f, p.y + ROUNDING}, {p.x + 0.5f, q.y - ROUNDING}, IM_COL32(255,255,255,40));
    dl->AddLine({p.x + ROUNDING, q.y - 0.5f}, {q.x - ROUNDING, q.y - 0.5f}, IM_COL32(0,0,0,120));
    dl->AddLine({q.x - 0.5f, p.y + ROUNDING}, {q.x - 0.5f, q.y - ROUNDING}, IM_COL32(0,0,0,100));
    dl->AddRect(p, q, IM_COL32(30, 30, 45, 255), ROUNDING, 0, 1.0f);

    // "VU" label — small font, left side, vertically centered
    if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
    const ImVec2 lblSz = ImGui::CalcTextSize("VU");
    dl->AddText({p.x + PAD_X, p.y + (boxH - lblSz.y) * 0.5f}, kClrTextLabel, "VU");
    if (g_fontKnobLabel) ImGui::PopFont();

    // LCD inner box
    const float lcdX = p.x + PAD_X + lblSz.x + LBL_GAP;
    const float lcdY = p.y + PAD_Y;
    const float lcdW = q.x - PAD_X - lcdX;
    const float lcdH = boxH - PAD_Y * 2.f;

    dl->AddLine({lcdX + 0.5f,        lcdY + 0.5f},         {lcdX + lcdW - 0.5f, lcdY + 0.5f},         IM_COL32(0,0,0,100));
    dl->AddLine({lcdX + 0.5f,        lcdY + 0.5f},         {lcdX + 0.5f,        lcdY + lcdH - 0.5f},  IM_COL32(0,0,0,80));
    dl->AddLine({lcdX + 0.5f,        lcdY + lcdH - 0.5f}, {lcdX + lcdW - 0.5f, lcdY + lcdH - 0.5f},  IM_COL32(255,255,255,50));
    dl->AddLine({lcdX + lcdW - 0.5f, lcdY + 0.5f},         {lcdX + lcdW - 0.5f, lcdY + lcdH - 0.5f},  IM_COL32(255,255,255,40));
    dl->AddRectFilled({lcdX, lcdY}, {lcdX + lcdW, lcdY + lcdH}, IM_COL32(14, 20, 24, 255), LCD_R);

    // Bar graphs inside the LCD box
    drawVU(dl,
           lcdX + LCD_PAD, lcdY + LCD_PAD,
           lcdW - LCD_PAD, lcdH - LCD_PAD * 2.f,
           lvlL, lvlR);
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
    const float btnMidY  = 11.f;
    const float knobMidY = 36.f;

    auto btnY = [&]() { ImGui::SetCursorPosY(btnMidY); };

    // ─── File buttons (native traffic lights sit to the left via CocoaHelpers) ──
    {
        const float by = 11.f;

        // Leave space for the 3 native traffic light buttons (~80px)
        ImGui::SetCursorPosX(80.f);
        ImGui::SetCursorPosY(by);
        if (SwitchButton("New##new",    "New"))    { if (state.onNew)   state.onNew();   }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        if (SwitchButton("Open##open",  "Open"))   { if (state.onOpen)  state.onOpen();  }
        ImGui::SameLine(0.f, 3.f);
        ImGui::SetCursorPosY(by);
        if (SwitchButton("Save##save", "Save", (int)state.isDirty, 1, {0.f, 0.f}, kClrAmber))
            { if (state.onSave) state.onSave(); }
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

    // Play — green LED when playing; click toggles play/pause
    btnY();
    const bool isPlaying = state.transport == Transport::Playing;
    if (SwitchButton("Play##play", "Play", (int)isPlaying, 1, {0.f, 0.f}, kClrLedGreen))
        { if (state.onPlayPause) state.onPlayPause(); }

    // Restart — rewind to beginning and play
    ImGui::SameLine();
    btnY();
    if (SwitchButton("Restart##rst", "Restart"))
        { if (state.onRestart) state.onRestart(); }

    if (!state.hasModule) ImGui::EndDisabled();

    ImGui::SetCursorPos({12.f, knobMidY});

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
        const float boxW = 220.f;
        const float boxH = knobSz;
        ImGui::SameLine(0.f, 6.f);
        ImGui::SetCursorPosY(knobMidY);
        const ImVec2 vuPos = ImGui::GetCursorScreenPos();
        drawVUWidget(dl, vuPos, boxW, boxH, state.vuLeft, state.vuRight);
        ImGui::Dummy({boxW, boxH});
    }

    // ─── Logo (top-right) ─────────────────────────────────────────────────────
    {
        static LogoView logo;
        ImVec2 winPos = ImGui::GetWindowPos();
        logo.draw(winPos.x, winPos.y, ImGui::GetWindowSize().x,
                  state.logoTex, state.logoW, state.logoH);
    }

    ImGui::End();
    ImGui::PopFont();
    popToolbarStyle();

    return y + h;
}
