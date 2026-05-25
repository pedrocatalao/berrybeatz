#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "imgui.h"
#include "RenderSettings.h"
#include "Theme.h"
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

enum class KnobLabelPos { None, Top, Bottom, Left, Right };

// ---------------------------------------------------------------------------
// KnobWidget
//   id            — ImGui string ID (also used for PushID)
//   value         — current value (read/write)
//   vmin, vmax    — inclusive range
//   editRequested — optional; set to true when user clicks without dragging
//   label         — optional text drawn near the knob
//   labelPos      — placement relative to knob body (default: Bottom)
//
// Returns true if *value was changed this frame.
// ---------------------------------------------------------------------------
inline bool KnobWidget(const char* id,
                       int*        value,
                       int         vmin,
                       int         vmax,
                       bool*       editRequested = nullptr,
                       bool        hideValue     = false,
                       bool        bipolar       = false,
                       const char* label         = nullptr,
                       KnobLabelPos labelPos     = KnobLabelPos::Bottom)
{
    ImGui::PushID(id);

    const float  SIZE    = 36.f;
    const float  LBL_GAP = 3.f;

    const bool hasLabel = label && label[0] && labelPos != KnobLabelPos::None;
    if (hasLabel && g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
    const ImVec2 lblSz = hasLabel ? ImGui::CalcTextSize(label) : ImVec2{0.f, 0.f};
    if (hasLabel && g_fontKnobLabel) ImGui::PopFont();

    // Full widget bounding box (knob + label)
    float totalW = SIZE, totalH = SIZE;
    if (hasLabel) {
        if (labelPos == KnobLabelPos::Top || labelPos == KnobLabelPos::Bottom) {
            totalW = SIZE > lblSz.x ? SIZE : lblSz.x;
            totalH = SIZE + LBL_GAP + lblSz.y;
        } else {
            totalW = SIZE + LBL_GAP + lblSz.x;
            totalH = SIZE > lblSz.y ? SIZE : lblSz.y;
        }
    }

    ImGui::BeginGroup();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    // Knob position within the widget (offset when label is top or left)
    ImVec2 knobPos = origin;
    if (hasLabel) {
        if (labelPos == KnobLabelPos::Top)  knobPos.y += lblSz.y + LBL_GAP;
        if (labelPos == KnobLabelPos::Left) knobPos.x += lblSz.x + LBL_GAP;
    }

    const ImVec2 center  = {knobPos.x + SIZE * 0.5f, knobPos.y + SIZE * 0.5f};
    const float  radius  = SIZE * 0.5f - 1.f;
    const float  trackR  = radius - 3.f;

    ImGui::SetCursorScreenPos(knobPos);
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
    dl->AddCircleFilled(center, radius, kClrKnobBg);
    dl->AddCircle(center, radius, kClrKnobBorder, 0, 1.5f);

    // Arc geometry: 270° sweep starting at 7 o'clock (135° = PI*0.75)
    const float SWEEP     = (float)(M_PI * 1.5);
    const float ANG_START = (float)(M_PI * 0.75);

    // Unlit track
    dl->PathArcTo(center, trackR, ANG_START, ANG_START + SWEEP, 32);
    dl->PathStroke(kClrKnobTrack, false, 2.5f);

    // Active (filled) arc
    float t = (vmax > vmin)
              ? (float)(*value - vmin) / (float)(vmax - vmin)
              : 0.f;

    const float ANG_CENTER = ANG_START + SWEEP * 0.5f;  // 12 o'clock
    float arcA, arcB;
    if (bipolar) {
        arcA = (t >= 0.5f) ? ANG_CENTER : ANG_START + t * SWEEP;
        arcB = (t >= 0.5f) ? ANG_START + t * SWEEP : ANG_CENTER;
    } else {
        arcA = ANG_START;
        arcB = ANG_START + t * SWEEP;
    }

    auto drawArc = [&](float a, float b) {
        if (b <= a) return;
        dl->PathArcTo(center, trackR, a, b, 32);
        dl->PathStroke(kClrKnobArc, false, 2.5f);

        if (g_glowKnobs > 0.f) {
            auto ga = [](int a) { return (uint8_t)(a * g_glowKnobs); };
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }, nullptr);
            dl->PathArcTo(center, trackR, a, b, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga( 3)), false, 22.0f);
            dl->PathArcTo(center, trackR, a, b, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga( 6)), false, 15.0f);
            dl->PathArcTo(center, trackR, a, b, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga(10)), false, 10.0f);
            dl->PathArcTo(center, trackR, a, b, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga(18)), false,  6.0f);
            dl->PathArcTo(center, trackR, a, b, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga(28)), false,  3.0f);
            float sx = center.x + trackR * cosf(a), sy = center.y + trackR * sinf(a);
            float ex = center.x + trackR * cosf(b), ey = center.y + trackR * sinf(b);
            for (auto [cx2, cy2] : {std::pair{sx,sy}, std::pair{ex,ey}}) {
                dl->AddCircleFilled({cx2, cy2}, 11.0f, IM_COL32(0, 220, 255, ga( 3)));
                dl->AddCircleFilled({cx2, cy2},  8.0f, IM_COL32(0, 220, 255, ga( 6)));
                dl->AddCircleFilled({cx2, cy2},  5.0f, IM_COL32(0, 220, 255, ga(10)));
                dl->AddCircleFilled({cx2, cy2},  3.0f, IM_COL32(0, 220, 255, ga(18)));
            }
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }, nullptr);
        }
    };
    drawArc(arcA, arcB);

    // Bipolar: small center tick at 12 o'clock
    if (bipolar) {
        float tx0 = center.x + (trackR - 3.f) * cosf(ANG_CENTER);
        float ty0 = center.y + (trackR - 3.f) * sinf(ANG_CENTER);
        float tx1 = center.x + (trackR + 3.f) * cosf(ANG_CENTER);
        float ty1 = center.y + (trackR + 3.f) * sinf(ANG_CENTER);
        dl->AddLine({tx0, ty0}, {tx1, ty1}, kClrKnobTick, 1.5f);
    }

    // Value text centered in the knob (suppressed when caller renders an edit field)
    if (!hideValue) {
        char buf[16];
        if (bipolar) {
            int offset = *value - (vmin + vmax) / 2;
            snprintf(buf, sizeof(buf), "%+d", offset);
            if (offset == 0) snprintf(buf, sizeof(buf), "0");
        } else {
            snprintf(buf, sizeof(buf), "%d", *value);
        }
        if (g_fontKnobValue) ImGui::PushFont(g_fontKnobValue);
        ImVec2 textSz = ImGui::CalcTextSize(buf);
        dl->AddText(
            {center.x - textSz.x * 0.5f, center.y - textSz.y * 0.5f},
            kClrTextValue,
            buf
        );
        if (g_fontKnobValue) ImGui::PopFont();
    }

    // ---- Label --------------------------------------------------------
    if (hasLabel) {
        ImVec2 lp;
        if (labelPos == KnobLabelPos::Top)
            lp = {origin.x + (totalW - lblSz.x) * 0.5f, origin.y};
        else if (labelPos == KnobLabelPos::Bottom)
            lp = {origin.x + (totalW - lblSz.x) * 0.5f, knobPos.y + SIZE + LBL_GAP};
        else if (labelPos == KnobLabelPos::Left)
            lp = {origin.x, origin.y + (SIZE - lblSz.y) * 0.5f};
        else  // Right
            lp = {knobPos.x + SIZE + LBL_GAP, origin.y + (SIZE - lblSz.y) * 0.5f};
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        dl->AddText(lp, kClrTextLabel, label);
        if (g_fontKnobLabel) ImGui::PopFont();
    }

    // Claim full bounding box so surrounding layout accounts for label space
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({totalW, totalH});

    ImGui::EndGroup();
    ImGui::PopID();
    return changed;
}

// KnobDisplay — read-only bipolar knob used for visual feedback (e.g. pitch bend).
// t is in [-1.0, 1.0] where 0.0 = center.
inline void KnobDisplay(const char* id, float t,
                        const char* label     = nullptr,
                        KnobLabelPos labelPos = KnobLabelPos::Bottom)
{
    ImGui::PushID(id);

    const float  SIZE    = 36.f;
    const float  LBL_GAP = 3.f;

    const bool hasLabel = label && label[0] && labelPos != KnobLabelPos::None;
    if (hasLabel && g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
    const ImVec2 lblSz = hasLabel ? ImGui::CalcTextSize(label) : ImVec2{0.f, 0.f};
    if (hasLabel && g_fontKnobLabel) ImGui::PopFont();

    float totalW = SIZE, totalH = SIZE;
    if (hasLabel) {
        if (labelPos == KnobLabelPos::Top || labelPos == KnobLabelPos::Bottom) {
            totalW = SIZE > lblSz.x ? SIZE : lblSz.x;
            totalH = SIZE + LBL_GAP + lblSz.y;
        } else {
            totalW = SIZE + LBL_GAP + lblSz.x;
            totalH = SIZE > lblSz.y ? SIZE : lblSz.y;
        }
    }

    ImGui::BeginGroup();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImVec2 knobPos = origin;
    if (hasLabel) {
        if (labelPos == KnobLabelPos::Top)  knobPos.y += lblSz.y + LBL_GAP;
        if (labelPos == KnobLabelPos::Left) knobPos.x += lblSz.x + LBL_GAP;
    }

    const ImVec2 center = {knobPos.x + SIZE * 0.5f, knobPos.y + SIZE * 0.5f};
    const float  radius = SIZE * 0.5f - 1.f;
    const float  trackR = radius - 3.f;

    ImGui::SetCursorScreenPos(knobPos);
    ImGui::Dummy({SIZE, SIZE});

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddCircleFilled(center, radius, kClrKnobBg);
    dl->AddCircle(center, radius, kClrKnobBorder, 0, 1.5f);

    const float SWEEP      = (float)(M_PI * 1.5);
    const float ANG_START  = (float)(M_PI * 0.75);
    const float ANG_CENTER = ANG_START + SWEEP * 0.5f;

    dl->PathArcTo(center, trackR, ANG_START, ANG_START + SWEEP, 32);
    dl->PathStroke(kClrKnobTrack, false, 2.5f);

    float tN = (t < -1.f ? -1.f : (t > 1.f ? 1.f : t)) * 0.5f + 0.5f;  // [0,1]
    float arcA = (tN >= 0.5f) ? ANG_CENTER             : ANG_START + tN * SWEEP;
    float arcB = (tN >= 0.5f) ? ANG_START + tN * SWEEP : ANG_CENTER;
    if (arcB > arcA) {
        dl->PathArcTo(center, trackR, arcA, arcB, 32);
        dl->PathStroke(kClrKnobArc, false, 2.5f);
        if (g_glowKnobs > 0.f) {
            auto ga = [](int a) { return (uint8_t)(a * g_glowKnobs); };
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }, nullptr);
            dl->PathArcTo(center, trackR, arcA, arcB, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga( 3)), false, 22.0f);
            dl->PathArcTo(center, trackR, arcA, arcB, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga( 6)), false, 15.0f);
            dl->PathArcTo(center, trackR, arcA, arcB, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga(10)), false, 10.0f);
            dl->PathArcTo(center, trackR, arcA, arcB, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga(18)), false,  6.0f);
            dl->PathArcTo(center, trackR, arcA, arcB, 32);
            dl->PathStroke(IM_COL32(0, 220, 255, ga(28)), false,  3.0f);
            float sx = center.x + trackR * cosf(arcA), sy = center.y + trackR * sinf(arcA);
            float ex = center.x + trackR * cosf(arcB), ey = center.y + trackR * sinf(arcB);
            for (auto [cx2, cy2] : {std::pair{sx,sy}, std::pair{ex,ey}}) {
                dl->AddCircleFilled({cx2, cy2}, 11.0f, IM_COL32(0, 220, 255, ga( 3)));
                dl->AddCircleFilled({cx2, cy2},  8.0f, IM_COL32(0, 220, 255, ga( 6)));
                dl->AddCircleFilled({cx2, cy2},  5.0f, IM_COL32(0, 220, 255, ga(10)));
                dl->AddCircleFilled({cx2, cy2},  3.0f, IM_COL32(0, 220, 255, ga(18)));
            }
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }, nullptr);
        }
    }

    // Center tick
    float tx0 = center.x + (trackR - 3.f) * cosf(ANG_CENTER);
    float ty0 = center.y + (trackR - 3.f) * sinf(ANG_CENTER);
    float tx1 = center.x + (trackR + 3.f) * cosf(ANG_CENTER);
    float ty1 = center.y + (trackR + 3.f) * sinf(ANG_CENTER);
    dl->AddLine({tx0, ty0}, {tx1, ty1}, kClrKnobTick, 1.5f);

    // Label
    if (hasLabel) {
        ImVec2 lp;
        if (labelPos == KnobLabelPos::Top)
            lp = {origin.x + (totalW - lblSz.x) * 0.5f, origin.y};
        else if (labelPos == KnobLabelPos::Bottom)
            lp = {origin.x + (totalW - lblSz.x) * 0.5f, knobPos.y + SIZE + LBL_GAP};
        else if (labelPos == KnobLabelPos::Left)
            lp = {origin.x, origin.y + (SIZE - lblSz.y) * 0.5f};
        else
            lp = {knobPos.x + SIZE + LBL_GAP, origin.y + (SIZE - lblSz.y) * 0.5f};
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        dl->AddText(lp, kClrTextLabel, label);
        if (g_fontKnobLabel) ImGui::PopFont();
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({totalW, totalH});

    ImGui::EndGroup();
    ImGui::PopID();
}

// KnobEdit — KnobWidget with click-to-type inline editing (same pattern as toolbar knobs).
// Bipolar knobs show and accept the offset from center in the edit field.
inline bool KnobEdit(const char* id, int* value, int vmin, int vmax,
                     bool bipolar     = false,
                     const char* label    = nullptr,
                     KnobLabelPos labelPos = KnobLabelPos::Bottom)
{
    static ImGuiID s_activeID = 0;
    static int     s_editVal  = 0;
    static bool    s_doFocus  = false;

    // Compute the same widget ID that KnobWidget's InvisibleButton will use
    ImGui::PushID(id);
    ImGuiID wid = ImGui::GetID("knob");
    ImGui::PopID();

    bool   editing = (s_activeID == wid);
    ImVec2 sp      = ImGui::GetCursorScreenPos();
    const float SIZE = 36.f, LBL_GAP = 3.f;

    // Full widget size — mirrors KnobWidget's calculation for cursor restoration
    const bool hasLabel = label && label[0] && labelPos != KnobLabelPos::None;
    if (hasLabel && g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
    const ImVec2 lblSz = hasLabel ? ImGui::CalcTextSize(label) : ImVec2{0.f, 0.f};
    if (hasLabel && g_fontKnobLabel) ImGui::PopFont();
    float totalW = SIZE, totalH = SIZE;
    if (hasLabel) {
        if (labelPos == KnobLabelPos::Top || labelPos == KnobLabelPos::Bottom) {
            totalW = SIZE > lblSz.x ? SIZE : lblSz.x;
            totalH = SIZE + LBL_GAP + lblSz.y;
        } else {
            totalW = SIZE + LBL_GAP + lblSz.x;
            totalH = SIZE > lblSz.y ? SIZE : lblSz.y;
        }
    }

    bool editReq = false;
    bool changed = KnobWidget(id, value, vmin, vmax, &editReq, editing, bipolar, label, labelPos);

    if (editReq) {
        s_activeID = wid;
        s_editVal  = bipolar ? (*value - (vmin + vmax) / 2) : *value;
        s_doFocus  = true;
    }

    if (editing) {
        char ibuf[16];
        snprintf(ibuf, sizeof(ibuf), "%d", s_editVal);
        ImVec2 textSz = ImGui::CalcTextSize(ibuf);
        const float iw = textSz.x + ImGui::GetTextLineHeight();

        // Knob center — same offset logic as KnobWidget
        ImVec2 knobPos = sp;
        if (hasLabel) {
            if (labelPos == KnobLabelPos::Top)  knobPos.y += lblSz.y + LBL_GAP;
            if (labelPos == KnobLabelPos::Left) knobPos.x += lblSz.x + LBL_GAP;
        }

        float editX = knobPos.x + (SIZE - textSz.x) * 0.5f;
        float editY = knobPos.y + (SIZE - textSz.y) * 0.5f;
        ImGui::SetCursorScreenPos({editX, editY});
        ImGui::SetNextItemWidth(iw);
        if (s_doFocus) { ImGui::SetKeyboardFocusHere(); s_doFocus = false; }

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text,           kClrTextValue);
        ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_NavHighlight,   IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    ImVec2(0.f, 0.f));

        ImGui::PushID(id);
        if (ImGui::InputInt("##ke", &s_editVal, 0, 0,
                ImGuiInputTextFlags_EnterReturnsTrue |
                ImGuiInputTextFlags_AutoSelectAll)) {
            int finalVal = bipolar ? ((vmin + vmax) / 2 + s_editVal) : s_editVal;
            finalVal = finalVal < vmin ? vmin : (finalVal > vmax ? vmax : finalVal);
            *value   = finalVal;
            changed  = true;
            s_activeID = 0;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            s_activeID = 0;
        ImGui::PopID();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(6);

        // Restore cursor to correct position after the full widget (including label space)
        ImGui::SetCursorScreenPos(sp);
        ImGui::Dummy({totalW, totalH});
    }

    return changed;
}
