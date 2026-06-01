#pragma once
#include "imgui.h"
#include <functional>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// FT2-style computer keyboard → note mapping (shared with pattern editor).
// Semi offsets are relative to baseOctave: lower row = +0..11, upper = +12..23.
struct PianoNoteKey { ImGuiKey key; int semi; };
inline constexpr PianoNoteKey kPianoNoteKeys[] = {
    {ImGuiKey_Z,0},{ImGuiKey_S,1},{ImGuiKey_X,2},{ImGuiKey_D,3},
    {ImGuiKey_C,4},{ImGuiKey_V,5},{ImGuiKey_G,6},{ImGuiKey_B,7},
    {ImGuiKey_H,8},{ImGuiKey_N,9},{ImGuiKey_J,10},{ImGuiKey_M,11},
    {ImGuiKey_Q,12},{ImGuiKey_2,13},{ImGuiKey_W,14},{ImGuiKey_3,15},
    {ImGuiKey_E,16},{ImGuiKey_R,17},{ImGuiKey_5,18},{ImGuiKey_T,19},
    {ImGuiKey_6,20},{ImGuiKey_Y,21},{ImGuiKey_7,22},{ImGuiKey_U,23},
};

// ─────────────────────────────────────────────────────────────────────────────
// Reusable piano keyboard widget — 5 octaves (notes 25–84), 120px tall.
//
// State: PianoKeyboardState holds playback + held-key display state.
//
// onPlay(note1based, key)  — key=128 for mouse drag, note-1 for computer key
// onStop(key)              — matching key from onPlay
// captureKeyboard          — set false to suppress computer-key shortcuts
//                            (use when the host window has its own key handlers)
// ─────────────────────────────────────────────────────────────────────────────

struct PianoKeyboardState {
    bool playHeld  {false};
    int  playNote  {-1};
    int  baseOctave{4};
    bool kbHeld[128]{};
};

inline void DrawPianoKeyboard(
    const char*                   id,
    float                         w,
    float                         h,
    PianoKeyboardState&           st,
    ImFont*                       fontMono,
    bool                          captureKeyboard,
    std::function<void(int, int)> onPlay,
    std::function<void(int)>      onStop)
{
    constexpr int kNoteFirst  = 25;
    constexpr int kNoteLast   = 84;
    constexpr int kOctaves    = 5;
    constexpr int kWhiteCount = kOctaves * 7;

    static const bool  kIsBlack[12]     = {false,true,false,true,false,false,true,false,true,false,true,false};
    static const float kBlackOffset[12] = {0,0.67f,0,0.83f,0,0,0.67f,0,0.74f,0,0.82f,0};

    constexpr float kPad = 3.f;

    ImVec2      origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      mp     = ImGui::GetIO().MousePos;

    ImGui::InvisibleButton(id, {w, h});

    origin.x += kPad;
    origin.y += kPad;
    w -= kPad * 6.f;
    h -= kPad * 2.f;

    float whiteW = w / (float)kWhiteCount;
    float blackW = whiteW * 0.65f;
    float blackH = h * 0.60f;
    bool kbActive      = ImGui::IsItemActive();
    bool kbDeactivated = ImGui::IsItemDeactivated();

    // Hit detection — black keys take priority
    int hitNote = -1;
    if (kbActive) {
        int wi = 0;
        for (int note = kNoteFirst; note <= kNoteLast && hitNote < 0; ++note) {
            int semi = (note - 1) % 12;
            if (!kIsBlack[semi]) { ++wi; continue; }
            float kx = origin.x + ((wi - 1) + kBlackOffset[semi]) * whiteW - blackW * 0.5f + 7.f;
            if (mp.x >= kx && mp.x < kx + blackW && mp.y >= origin.y && mp.y < origin.y + blackH)
                hitNote = note;
        }
        if (hitNote < 0) {
            int wi2 = 0;
            for (int note = kNoteFirst; note <= kNoteLast && hitNote < 0; ++note) {
                int semi = (note - 1) % 12;
                if (kIsBlack[semi]) continue;
                float kx = origin.x + wi2 * whiteW;
                if (mp.x >= kx && mp.x < kx + whiteW - 1.f &&
                    mp.y >= origin.y && mp.y < origin.y + h)
                    hitNote = note;
                ++wi2;
            }
        }
    }

    // ── White keys ────────────────────────────────────────────────────────────
    int whiteIndex = 0;
    for (int note = kNoteFirst; note <= kNoteLast; ++note) {
        int semi = (note - 1) % 12;
        if (kIsBlack[semi]) continue;

        float  kx   = origin.x + whiteIndex * whiteW;
        ImVec2 kMin = {kx,               origin.y};
        ImVec2 kMax = {kx + whiteW - 1.f, origin.y + h};

        bool isHit  = (note == hitNote);
        bool isPlay = (note == st.playNote && st.playHeld);
        bool isKb   = (note >= 1 && note <= 128 && st.kbHeld[note - 1]);
        bool active = isHit || isPlay || isKb;
        bool hov    = (!kbActive && mp.x >= kMin.x && mp.x < kMax.x &&
                                    mp.y >= kMin.y && mp.y < kMax.y);

        // Gradient fill
        ImU32 topCol = active ? IM_COL32(140,190,255,255) : hov ? IM_COL32(225,235,255,255) : IM_COL32(248,248,254,255);
        ImU32 botCol = active ? IM_COL32( 90,150,230,255) : hov ? IM_COL32(195,210,240,255) : IM_COL32(200,198,215,255);
        dl->AddRectFilledMultiColor(kMin, kMax, topCol, topCol, botCol, botCol);
        // Border
        dl->AddRect(kMin, kMax, IM_COL32(70, 68, 90, 255), 0.f, 0, 0.5f);
        // Left highlight
        dl->AddLine({kMin.x + 1.f, kMin.y + 1.f}, {kMin.x + 1.f, kMax.y - 2.f}, IM_COL32(255,255,255,160), 1.f);
        // Bottom shadow
        dl->AddLine({kMin.x + 1.f, kMax.y - 1.f}, {kMax.x - 1.f, kMax.y - 1.f}, IM_COL32(80,75,100,200), 1.f);

        if (fontMono) {
            // Computer keyboard shortcut label
            static const char* kLo[] = {"Z","","X","","C","V","","B","","N","","M"};
            static const char* kHi[] = {"Q","","W","","E","R","","T","","Y","","U"};
            int rel = (note - 1) - st.baseOctave * 12;
            const char* lbl = nullptr;
            if (rel >= 0  && rel < 12 && kLo[rel][0]) lbl = kLo[rel];
            if (rel >= 12 && rel < 24 && kHi[rel - 12][0]) lbl = kHi[rel - 12];
            if (lbl) {
                ImVec2 tsz = fontMono->CalcTextSizeA(8.f, FLT_MAX, 0.f, lbl);
                dl->AddText(fontMono, 8.f,
                    {kx + (whiteW - tsz.x) * 0.5f, kMin.y + 3.f},
                    IM_COL32(80, 80, 110, 200), lbl);
            }

            // Note name on C keys
            if (semi == 0) {
                char nbuf[8];
                snprintf(nbuf, sizeof(nbuf), "C%d", (note - 1) / 12);
                ImVec2 tsz = fontMono->CalcTextSizeA(8.f, FLT_MAX, 0.f, nbuf);
                if (tsz.x < whiteW - 2.f)
                    dl->AddText(fontMono, 8.f,
                        {kx + (whiteW - tsz.x) * 0.5f, kMax.y - tsz.y - 2.f},
                        IM_COL32(0, 60, 0, 200), nbuf);
            }
        }
        ++whiteIndex;
    }

    // ── Black keys (drawn on top) ─────────────────────────────────────────────
    whiteIndex = 0;
    for (int note = kNoteFirst; note <= kNoteLast; ++note) {
        int semi = (note - 1) % 12;
        if (!kIsBlack[semi]) { ++whiteIndex; continue; }

        float  kx   = origin.x + ((whiteIndex - 1) + kBlackOffset[semi]) * whiteW - blackW * 0.5f + 7.f;
        ImVec2 kMin = {kx,          origin.y};
        ImVec2 kMax = {kx + blackW, origin.y + blackH};

        bool isHit  = (note == hitNote);
        bool isPlay = (note == st.playNote && st.playHeld);
        bool isKb   = (note >= 1 && note <= 128 && st.kbHeld[note - 1]);
        bool active = isHit || isPlay || isKb;
        bool hov    = (!kbActive && mp.x >= kMin.x && mp.x < kMax.x &&
                                    mp.y >= kMin.y && mp.y < kMax.y);

        // Gradient fill
        ImU32 bkTop = active ? IM_COL32(50, 90,170,255) : hov ? IM_COL32(55,55,75,255) : IM_COL32(50,50,65,255);
        ImU32 bkBot = active ? IM_COL32(20, 50,110,255) : hov ? IM_COL32(18,18,28,255) : IM_COL32(12,12,18,255);
        dl->AddRectFilledMultiColor(kMin, kMax, bkTop, bkTop, bkBot, bkBot);
        // Shine strip at top
        dl->AddRectFilled({kMin.x+2.f, kMin.y+2.f}, {kMax.x-2.f, kMin.y+7.f}, IM_COL32(90,90,120, active?60:110));
        // Left highlight
        dl->AddLine({kMin.x+1.f, kMin.y+1.f}, {kMin.x+1.f, kMax.y-2.f}, IM_COL32(90,90,120,160), 1.f);
        // Right shadow
        dl->AddLine({kMax.x-1.f, kMin.y+1.f}, {kMax.x-1.f, kMax.y-1.f}, IM_COL32(0,0,0,200), 1.f);
        // Border
        dl->AddRect(kMin, kMax, IM_COL32(0,0,0,255), 0.f, 0, 0.5f);

        if (fontMono) {
            static const char* kLo[] = {"","S","","D","","","G","","H","","J",""};
            static const char* kHi[] = {"","2","","3","","","5","","6","","7",""};
            int rel = (note - 1) - st.baseOctave * 12;
            const char* lbl = nullptr;
            if (rel >= 0  && rel < 12 && kLo[rel][0]) lbl = kLo[rel];
            if (rel >= 12 && rel < 24 && kHi[rel - 12][0]) lbl = kHi[rel - 12];
            if (lbl) {
                ImVec2 tsz = fontMono->CalcTextSizeA(7.f, FLT_MAX, 0.f, lbl);
                dl->AddText(fontMono, 7.f,
                    {kx + (blackW - tsz.x) * 0.5f, kMin.y + 2.f},
                    IM_COL32(180, 180, 210, 200), lbl);
            }
        }
    }

    // ── Mouse play / stop ─────────────────────────────────────────────────────
    if (hitNote > 0 && hitNote != st.playNote) {
        if (st.playHeld && onStop) onStop(128);
        if (onPlay) onPlay(hitNote, 128);
        st.playHeld = true;
        st.playNote = hitNote;
    }
    if (kbDeactivated && st.playHeld) {
        if (onStop) onStop(128);
        st.playHeld = false;
        st.playNote = -1;
    }

    // ── Computer keyboard play / stop ─────────────────────────────────────────
    if (captureKeyboard && !ImGui::GetIO().WantTextInput) {
        for (auto& k : kPianoNoteKeys) {
            int note = st.baseOctave * 12 + k.semi + 1;
            if (note < 1 || note > 128) continue;
            if (ImGui::IsKeyPressed(k.key, false) && !st.kbHeld[note - 1]) {
                if (onPlay) onPlay(note, note - 1);
                st.kbHeld[note - 1] = true;
            }
            if (ImGui::IsKeyReleased(k.key) && st.kbHeld[note - 1]) {
                if (onStop) onStop(note - 1);
                st.kbHeld[note - 1] = false;
            }
        }
    }
}
