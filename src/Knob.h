#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "imgui.h"
#include <unordered_map>
#include <cmath>

// ---------------------------------------------------------------------------
// Per-widget drag state (stored in a function-local static map to avoid ODR
// violations in a header-only translation unit).
// ---------------------------------------------------------------------------
struct KnobState {
    int   startY    {0};
    int   startVal  {0};
    float totalDY   {0.f};
    bool  dragged   {false};
    float scrollAcc {0.f};  // fractional scroll accumulator
};

inline KnobState& knobState(ImGuiID id)
{
    static std::unordered_map<ImGuiID, KnobState> s;
    return s[id];
}

// ---------------------------------------------------------------------------
// KnobWidget
//   id            — ImGui string ID (also used for PushID)
//   value         — current value (read/write)
//   vmin, vmax    — inclusive range
//   editRequested — optional; set to true when user clicks without dragging
//
// Returns true if *value was changed this frame.
// ---------------------------------------------------------------------------
inline bool KnobWidget(const char* id,
                       int*        value,
                       int         vmin,
                       int         vmax,
                       bool*       editRequested = nullptr,
                       bool        hideValue     = false)
{
    ImGui::PushID(id);

    const float   SIZE    = 36.f;
    const ImVec2  pos     = ImGui::GetCursorScreenPos();
    const ImVec2  center  = ImVec2(pos.x + SIZE * 0.5f, pos.y + SIZE * 0.5f);
    const float   radius  = SIZE * 0.5f - 1.f;       // body radius
    const float   trackR  = radius - 3.f;             // arc track radius

    // Reserve space and create invisible interaction target
    ImGuiID widgetID = ImGui::GetID("knob");
    ImGui::InvisibleButton("knob", ImVec2(SIZE, SIZE));
    bool changed = false;

    KnobState& st = knobState(widgetID);

    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();

    // ---- Mouse drag (vertical) ----------------------------------------
    if (ImGui::IsItemActivated()) {
        st.startY   = (int)ImGui::GetIO().MousePos.y;
        st.startVal = *value;
        st.totalDY  = 0.f;
        st.dragged  = false;
    }

    if (active) {
        float dy = (float)(st.startY - (int)ImGui::GetIO().MousePos.y); // up = positive
        st.totalDY += ImGui::GetIO().MouseDelta.y;  // accumulate for "dragged" threshold

        bool fine = ImGui::GetIO().KeyCtrl;
        float sensitivity = fine ? 5.f : 1.f;

        int range = vmax - vmin;
        if (range > 0) {
            int newVal = st.startVal + (int)(dy / sensitivity * (float)range / 100.f);
            newVal = newVal < vmin ? vmin : (newVal > vmax ? vmax : newVal);
            if (newVal != *value) { *value = newVal; changed = true; }
        }

        if (std::abs(st.totalDY) >= 3.f) st.dragged = true;
    }

    if (ImGui::IsItemDeactivated()) {
        if (!st.dragged && editRequested) *editRequested = true;
    }

    // ---- Mouse wheel --------------------------------------------------
    // Same sensitivity formula as vertical drag: range/100 per unit,
    // Ctrl for fine mode. Float accumulator preserves trackpad sub-steps.
    if (hovered && ImGui::GetIO().MouseWheel != 0.f) {
        bool  fine        = ImGui::GetIO().KeyCtrl;
        float sensitivity = fine ? 5.f : 1.f;
        int   range       = vmax - vmin;
        if (range > 0) {
            st.scrollAcc += ImGui::GetIO().MouseWheel / sensitivity * (float)range / 100.f;
            int delta = (int)st.scrollAcc;
            st.scrollAcc -= (float)delta;
            if (delta != 0) {
                int newVal = *value + delta;
                newVal = newVal < vmin ? vmin : (newVal > vmax ? vmax : newVal);
                if (newVal != *value) { *value = newVal; changed = true; }
            }
        }
    }

    // ---- Drawing -------------------------------------------------------
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Body
    dl->AddCircleFilled(center, radius, IM_COL32(14, 14, 34, 255));
    dl->AddCircle(center, radius, IM_COL32(64, 64, 114, 255), 0, 1.5f);

    // Arc geometry
    // 270° sweep starting at 7 o'clock: angle_start = 135° = PI*0.75
    const float SWEEP     = (float)(M_PI * 1.5);   // 270° in radians
    const float ANG_START = (float)(M_PI * 0.75);  // 135° = 7 o'clock

    // Unlit track
    dl->PathArcTo(center, trackR, ANG_START, ANG_START + SWEEP, 32);
    dl->PathStroke(IM_COL32(46, 46, 90, 255), false, 2.5f);

    // Active (filled) arc
    float t = (vmax > vmin)
              ? (float)(*value - vmin) / (float)(vmax - vmin)
              : 0.f;
    float angEnd = ANG_START + t * SWEEP;
    if (angEnd > ANG_START) {
        // Glow passes — additive blend
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }, nullptr);
        dl->PathArcTo(center, trackR, ANG_START, angEnd, 32);
        dl->PathStroke(IM_COL32(0, 220, 255,  8), false, 12.0f);
        dl->PathArcTo(center, trackR, ANG_START, angEnd, 32);
        dl->PathStroke(IM_COL32(0, 220, 255, 14), false,  8.0f);
        dl->PathArcTo(center, trackR, ANG_START, angEnd, 32);
        dl->PathStroke(IM_COL32(0, 220, 255, 22), false,  5.0f);
        dl->PathArcTo(center, trackR, ANG_START, angEnd, 32);
        dl->PathStroke(IM_COL32(0, 220, 255, 35), false,  3.0f);
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }, nullptr);
        // Normal arc on top
        dl->PathArcTo(center, trackR, ANG_START, angEnd, 32);
        dl->PathStroke(IM_COL32(0, 220, 255, 225), false, 2.5f);
    }

    // Value text centered in the knob (suppressed when caller renders an edit field)
    if (!hideValue) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", *value);
        ImVec2 textSz = ImGui::CalcTextSize(buf);
        dl->AddText(
            ImVec2(center.x - textSz.x * 0.5f, center.y - textSz.y * 0.5f),
            IM_COL32(208, 208, 80, 255),
            buf
        );
    }

    ImGui::PopID();
    return changed;
}
