#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "imgui.h"
#include "RenderSettings.h"
#include "Theme.h"
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <cstdio>

// ── Per-widget drag state ─────────────────────────────────────────────────────
struct KnobState {
    int   startY    {0};
    int   startVal  {0};
    float totalDY   {0.f};
    bool  dragged   {false};
    float scrollAcc {0.f};
};

inline KnobState& knobState(ImGuiID id)
{
    static std::unordered_map<ImGuiID, KnobState> s;
    return s[id];
}

enum class KnobLabelPos { None, Top, Bottom, Left, Right };

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr float KNOB_DIAL_D     = 26.f;   // dial circle diameter
static constexpr float KNOB_MARGIN     = 5.f;    // top/bottom inset
static constexpr float KNOB_BTN_H     = KNOB_DIAL_D + KNOB_MARGIN * 2.f;  // 36px
static constexpr float KNOB_PAD_TOP   = 3.f;    // label from top of box
static constexpr float KNOB_LBL_LCD_G = 2.f;    // label bottom → LCD top
static constexpr float KNOB_PAD_X     = 4.f;    // left padding (label/LCD)
static constexpr float KNOB_MID_GAP   = 6.f;    // left column → dial
static constexpr float KNOB_PAD_R     = 4.f;    // right padding after dial
static constexpr float KNOB_ROUNDING  = 1.5f;
static constexpr float KNOB_LCD_W     = 36.f;   // fixed LCD width
static constexpr float KNOB_LCD_H     = 13.f;   // fixed LCD height

// ── Arc / glow helper ─────────────────────────────────────────────────────────
static void knobDrawArc(ImDrawList* dl, ImVec2 center, float trackR,
                        float arcA, float arcB)
{
    if (arcB <= arcA) return;
    dl->PathArcTo(center, trackR, arcA, arcB, 32);
    dl->PathStroke(IM_COL32(100, 220, 160, 100), false, 2.f);

    if (g_glowKnobs > 0.f) {
        auto ga = [](int a) { return (uint8_t)(a * g_glowKnobs); };
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }, nullptr);
        dl->PathArcTo(center, trackR, arcA, arcB, 32);
        dl->PathStroke(IM_COL32(100, 220, 160, ga( 3)), false, 22.0f);
        dl->PathArcTo(center, trackR, arcA, arcB, 32);
        dl->PathStroke(IM_COL32(100, 220, 160, ga( 6)), false, 15.0f);
        dl->PathArcTo(center, trackR, arcA, arcB, 32);
        dl->PathStroke(IM_COL32(100, 220, 160, ga(10)), false, 10.0f);
        dl->PathArcTo(center, trackR, arcA, arcB, 32);
        dl->PathStroke(IM_COL32(100, 220, 160, ga(18)), false,  6.0f);
        dl->PathArcTo(center, trackR, arcA, arcB, 32);
        dl->PathStroke(IM_COL32(100, 220, 160, ga(28)), false,  3.0f);
        float sx = center.x + trackR * cosf(arcA), sy = center.y + trackR * sinf(arcA);
        float ex = center.x + trackR * cosf(arcB), ey = center.y + trackR * sinf(arcB);
        for (auto [cx2, cy2] : {std::pair{sx,sy}, std::pair{ex,ey}}) {
            dl->AddCircleFilled({cx2, cy2}, 11.f, IM_COL32(100, 220, 160, ga( 3)));
            dl->AddCircleFilled({cx2, cy2},  8.f, IM_COL32(100, 220, 160, ga( 6)));
            dl->AddCircleFilled({cx2, cy2},  5.f, IM_COL32(100, 220, 160, ga(10)));
            dl->AddCircleFilled({cx2, cy2},  3.f, IM_COL32(100, 220, 160, ga(18)));
        }
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }, nullptr);
    }
}

// Dial: hardware-style knob body with arc outside, indicator, and rim glow.
static void knobDrawDial(ImDrawList* dl, ImVec2 center, float t, bool bipolar)
{
    // bodyR: the physical knob circle (smaller, leaves room for arc outside)
    // arcR:  the range arc sits in the gap between body and bounding edge
    const float bodyR = KNOB_DIAL_D * 0.5f - 4.f;   // ~9px
    const float arcR  = KNOB_DIAL_D * 0.5f - 1.f;   // ~12px

    const float SWEEP      = (float)(M_PI * 1.5);
    const float ANG_START  = (float)(M_PI * 0.75);
    const float ANG_CENTER = ANG_START + SWEEP * 0.5f;
    const float indAng     = ANG_START + t * SWEEP;

    // ── Range arc (panel ring outside the knob body) ──────────────────────────
    dl->PathArcTo(center, arcR, ANG_START, ANG_START + SWEEP, 32);
    dl->PathStroke(kClrLcdBg, false, 2.f);

    float arcA, arcB;
    if (bipolar) {
        arcA = (t >= 0.5f) ? ANG_CENTER            : ANG_START + t * SWEEP;
        arcB = (t >= 0.5f) ? ANG_START + t * SWEEP : ANG_CENTER;
    } else {
        arcA = ANG_START;
        arcB = ANG_START + t * SWEEP;
    }
    knobDrawArc(dl, center, arcR, arcA, arcB);

    // Bipolar centre tick on the arc ring
    if (bipolar) {
        float tx0 = center.x + (arcR - 2.f) * cosf(ANG_CENTER);
        float ty0 = center.y + (arcR - 2.f) * sinf(ANG_CENTER);
        float tx1 = center.x + (arcR + 2.f) * cosf(ANG_CENTER);
        float ty1 = center.y + (arcR + 2.f) * sinf(ANG_CENTER);
        dl->AddLine({tx0, ty0}, {tx1, ty1}, kClrKnobTick, 1.5f);
    }

    // ── Knob body ─────────────────────────────────────────────────────────────
    dl->AddCircleFilled(center, bodyR + 1.f, IM_COL32(  6,  6, 14, 255));  // rim shadow
    dl->AddCircleFilled(center, bodyR,       IM_COL32( 28, 28, 44, 255));  // main body

    // ── 3D ridge indicator ────────────────────────────────────────────────────
    // Three parallel lines — shadow / surface / highlight — give the impression
    // of a raised groove pressed into the knob face.
    float tipX  = center.x + (bodyR - 1.5f) * cosf(indAng);
    float tipY  = center.y + (bodyR - 1.5f) * sinf(indAng);
    float baseX = center.x + 2.f * cosf(indAng);
    float baseY = center.y + 2.f * sinf(indAng);

    // Perpendicular direction for offset
    float px = -sinf(indAng);
    float py =  cosf(indAng);
    constexpr float OFF = 0.7f;

    dl->AddLine({baseX + px*OFF, baseY + py*OFF},
                {tipX  + px*OFF, tipY  + py*OFF},
                IM_COL32( 0,  0,  0, 130), 1.f);   // shadow side
    dl->AddLine({baseX, baseY}, {tipX, tipY},
                IM_COL32(110, 125, 118, 210), 1.5f); // ridge surface (muted)
    dl->AddLine({baseX - px*OFF, baseY - py*OFF},
                {tipX  - px*OFF, tipY  - py*OFF},
                IM_COL32(180, 200, 190, 100), 0.8f); // highlight side

    // ── Glow from behind the handle ───────────────────────────────────────────
    // Additive bloom centred at the body rim — light bleeds outward into the arc
    // gap, as if the indicator is a backlit element inside the knob.
    if (g_glowKnobs > 0.f) {
        auto ga = [](int a) { return (uint8_t)(a * g_glowKnobs); };
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }, nullptr);

        // Soft glow along the indicator (backlit handle)
        dl->AddLine({baseX, baseY}, {tipX, tipY},
                    IM_COL32(100, 220, 160, ga(55)), 3.f);

        // Radial bloom at the rim — "light from behind" bleeding around the edge
        float rimX = center.x + bodyR * cosf(indAng);
        float rimY = center.y + bodyR * sinf(indAng);
        dl->AddCircleFilled({rimX, rimY},  9.f, IM_COL32(100, 220, 160, ga( 4)));
        dl->AddCircleFilled({rimX, rimY},  6.f, IM_COL32(100, 220, 160, ga(12)));
        dl->AddCircleFilled({rimX, rimY},  3.5f, IM_COL32(100, 220, 160, ga(28)));
        dl->AddCircleFilled({rimX, rimY},  1.5f, IM_COL32(100, 220, 160, ga(70)));

        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }, nullptr);
    }
}

// SwitchButton-style outer box.
static void knobDrawBox(ImDrawList* dl, ImVec2 p, float w, float h, bool held, bool hovered)
{
    ImU32 bodyCol = held    ? IM_COL32(50, 50, 70, 255)
                 : hovered  ? IM_COL32(38, 38, 55, 255)
                            : IM_COL32(25, 25, 38, 255);
    dl->AddRectFilled(p, {p.x + w, p.y + h}, bodyCol, KNOB_ROUNDING);
    if (!held)
        dl->AddRectFilled(p, {p.x + w, p.y + h * 0.5f},
                          IM_COL32(255, 255, 255, 14), KNOB_ROUNDING);
    if (!held) {
        dl->AddLine({p.x + KNOB_ROUNDING,     p.y + 0.5f},
                    {p.x + w - KNOB_ROUNDING, p.y + 0.5f},     IM_COL32(255,255,255,55));
        dl->AddLine({p.x + 0.5f, p.y + KNOB_ROUNDING},
                    {p.x + 0.5f, p.y + h - KNOB_ROUNDING},     IM_COL32(255,255,255,40));
        dl->AddLine({p.x + KNOB_ROUNDING,     p.y + h - 0.5f},
                    {p.x + w - KNOB_ROUNDING, p.y + h - 0.5f}, IM_COL32(0,0,0,120));
        dl->AddLine({p.x + w - 0.5f, p.y + KNOB_ROUNDING},
                    {p.x + w - 0.5f, p.y + h - KNOB_ROUNDING}, IM_COL32(0,0,0,100));
    } else {
        dl->AddLine({p.x + KNOB_ROUNDING,     p.y + 0.5f},
                    {p.x + w - KNOB_ROUNDING, p.y + 0.5f},     IM_COL32(0,0,0,100));
        dl->AddLine({p.x + 0.5f, p.y + KNOB_ROUNDING},
                    {p.x + 0.5f, p.y + h - KNOB_ROUNDING},     IM_COL32(0,0,0,80));
        dl->AddLine({p.x + KNOB_ROUNDING,     p.y + h - 0.5f},
                    {p.x + w - KNOB_ROUNDING, p.y + h - 0.5f}, IM_COL32(255,255,255,35));
        dl->AddLine({p.x + w - 0.5f, p.y + KNOB_ROUNDING},
                    {p.x + w - 0.5f, p.y + h - KNOB_ROUNDING}, IM_COL32(255,255,255,25));
    }
    dl->AddRect(p, {p.x + w, p.y + h}, IM_COL32(30, 30, 45, 255), KNOB_ROUNDING, 0, 1.f);
}

// ── Layout helper (shared between KnobWidget and KnobEdit) ────────────────────
//
// Left column: label (top) stacked above LCD (bottom), both left-aligned at PAD_X.
// Right column: dial circle, full box height, centred vertically.
//
//  ┌────────────────────────────────────────────────┐
//  │ Label                          │               │
//  │ [ LCD value                  ] │  ( ◷ dial )   │
//  └────────────────────────────────────────────────┘
//
// Outputs: totalW, lcdX offset from box left, lcdY offset from box top, lcdH, lcdW.
static void knobCalcLayout(ImVec2 lblSz, bool hasLabel,
                           float& totalW, float& lcdX, float& lcdY)
{
    float leftW = std::max(hasLabel ? lblSz.x : 0.f, KNOB_LCD_W);
    totalW = KNOB_PAD_X + leftW + KNOB_MID_GAP + KNOB_DIAL_D + KNOB_PAD_R;

    lcdX = KNOB_PAD_X;

    float lblH    = hasLabel ? lblSz.y : 0.f;
    float topUsed = hasLabel ? (KNOB_PAD_TOP + lblH + KNOB_LBL_LCD_G) : KNOB_PAD_TOP;
    lcdY = topUsed;
}

// ── KnobWidget ────────────────────────────────────────────────────────────────
//
// Layout:  [ Label (top-left) / LCD (below label) ]  [ dial (right, full height) ]
//
// Returns true if *value changed.
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

    const bool  hasLabel = label && label[0] && labelPos != KnobLabelPos::None;
    ImVec2      lblSz    = {0.f, 0.f};
    if (hasLabel) {
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        lblSz = ImGui::CalcTextSize(label);
        if (g_fontKnobLabel) ImGui::PopFont();
    }

    float totalW, lcdX, lcdY;
    knobCalcLayout(lblSz, hasLabel, totalW, lcdX, lcdY);
    const float totalH = KNOB_BTN_H;

    ImGui::BeginGroup();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGuiID widgetID = ImGui::GetID("knob");
    ImGui::InvisibleButton("knob", {totalW, totalH});
    bool changed = false;

    KnobState& st      = knobState(widgetID);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();

    // ── Drag ─────────────────────────────────────────────────────────────────
    if (ImGui::IsItemActivated()) {
        st.startY   = (int)ImGui::GetIO().MousePos.y;
        st.startVal = *value;
        st.totalDY  = 0.f;
        st.dragged  = false;
    }
    if (active) {
        float dy   = (float)(st.startY - (int)ImGui::GetIO().MousePos.y);
        st.totalDY += ImGui::GetIO().MouseDelta.y;
        float sens  = ImGui::GetIO().KeyCtrl ? 5.f : 1.f;
        int   range = vmax - vmin;
        if (range > 0) {
            int newVal = std::clamp(st.startVal + (int)(dy / sens * range / 100.f), vmin, vmax);
            if (newVal != *value) { *value = newVal; changed = true; }
        }
        if (std::abs(st.totalDY) >= 3.f) st.dragged = true;
    }
    if (ImGui::IsItemDeactivated() && !st.dragged && editRequested)
        *editRequested = true;

    // ── Scroll wheel ──────────────────────────────────────────────────────────
    if (hovered && ImGui::GetIO().MouseWheel != 0.f) {
        float sens  = ImGui::GetIO().KeyCtrl ? 5.f : 1.f;
        int   range = vmax - vmin;
        if (range > 0) {
            st.scrollAcc += ImGui::GetIO().MouseWheel / sens * range / 100.f;
            int delta = (int)st.scrollAcc;
            st.scrollAcc -= (float)delta;
            if (delta) {
                int newVal = std::clamp(*value + delta, vmin, vmax);
                if (newVal != *value) { *value = newVal; changed = true; }
            }
        }
    }

    // ── Drawing ───────────────────────────────────────────────────────────────
    ImDrawList* dl = ImGui::GetWindowDrawList();
    knobDrawBox(dl, p, totalW, totalH, active, hovered);

    // Label centred over the LCD field
    if (hasLabel) {
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        float lx = p.x + lcdX + (KNOB_LCD_W - lblSz.x) * 0.5f;
        dl->AddText({lx, p.y + KNOB_PAD_TOP}, kClrTextLabel, label);
        if (g_fontKnobLabel) ImGui::PopFont();
    }

    // LCD housing (below label)
    float absLcdX = p.x + lcdX;
    float absLcdY = p.y + lcdY;
    dl->AddRectFilled({absLcdX - 1.f, absLcdY - 1.f},
                      {absLcdX + KNOB_LCD_W + 1.f, absLcdY + KNOB_LCD_H + 1.f},
                      IM_COL32(0, 0, 0, 180), 2.f);
    dl->AddRectFilled({absLcdX, absLcdY},
                      {absLcdX + KNOB_LCD_W, absLcdY + KNOB_LCD_H},
                      kClrLcdBg, 1.5f);

    if (!hideValue) {
        char buf[16];
        if (bipolar) {
            int off = *value - (vmin + vmax) / 2;
            snprintf(buf, sizeof(buf), off == 0 ? "0" : "%+d", off);
        } else {
            snprintf(buf, sizeof(buf), "%d", *value);
        }
        ImVec2 textSz = ImGui::CalcTextSize(buf);
        dl->PushClipRect({absLcdX, absLcdY}, {absLcdX + KNOB_LCD_W, absLcdY + KNOB_LCD_H}, true);
        dl->AddText({absLcdX + (KNOB_LCD_W - textSz.x) * 0.5f,
                     absLcdY + (KNOB_LCD_H - textSz.y) * 0.5f},
                    kClrLcdText, buf);
        dl->PopClipRect();
    }

    // Dial (right side, vertically centred)
    float dialCX = p.x + KNOB_PAD_X + (totalW - KNOB_PAD_X - KNOB_PAD_R - KNOB_DIAL_D) + KNOB_DIAL_D * 0.5f;
    float dialCY = p.y + totalH * 0.5f;
    float t = (vmax > vmin) ? (float)(*value - vmin) / (float)(vmax - vmin) : 0.f;
    knobDrawDial(dl, {dialCX, dialCY}, t, bipolar);

    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({totalW, totalH});
    ImGui::EndGroup();
    ImGui::PopID();
    return changed;
}

// ── KnobDisplay ───────────────────────────────────────────────────────────────
// Read-only bipolar arc indicator. t in [-1, 1].
inline void KnobDisplay(const char* id, float t,
                        const char*  label    = nullptr,
                        KnobLabelPos labelPos = KnobLabelPos::Bottom)
{
    ImGui::PushID(id);

    const bool  hasLabel = label && label[0] && labelPos != KnobLabelPos::None;
    ImVec2      lblSz    = {0.f, 0.f};
    if (hasLabel) {
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        lblSz = ImGui::CalcTextSize(label);
        if (g_fontKnobLabel) ImGui::PopFont();
    }

    // Display-only: just label on left + dial on right, no LCD
    float leftW = hasLabel ? lblSz.x : 0.f;
    float totalW = KNOB_PAD_X + leftW + KNOB_MID_GAP + KNOB_DIAL_D + KNOB_PAD_R;
    float totalH = KNOB_BTN_H;

    ImGui::BeginGroup();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy({totalW, totalH});

    ImDrawList* dl = ImGui::GetWindowDrawList();
    knobDrawBox(dl, p, totalW, totalH, false, false);

    if (hasLabel) {
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        dl->AddText({p.x + KNOB_PAD_X, p.y + (totalH - lblSz.y) * 0.5f},
                    kClrTextLabel, label);
        if (g_fontKnobLabel) ImGui::PopFont();
    }

    float tN     = std::clamp(t, -1.f, 1.f) * 0.5f + 0.5f;
    float dialCX = p.x + totalW - KNOB_PAD_R - KNOB_DIAL_D * 0.5f;
    float dialCY = p.y + totalH * 0.5f;
    knobDrawDial(dl, {dialCX, dialCY}, tN, true);

    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({totalW, totalH});
    ImGui::EndGroup();
    ImGui::PopID();
}

// ── KnobEdit ─────────────────────────────────────────────────────────────────
// KnobWidget with click-to-type editing inside the LCD housing.
inline bool KnobEdit(const char* id, int* value, int vmin, int vmax,
                     bool        bipolar  = false,
                     const char* label    = nullptr,
                     KnobLabelPos labelPos = KnobLabelPos::Bottom)
{
    static ImGuiID s_activeID = 0;
    static int     s_editVal  = 0;
    static bool    s_doFocus  = false;

    ImGui::PushID(id);
    ImGuiID wid = ImGui::GetID("knob");
    ImGui::PopID();

    const bool  hasLabel = label && label[0] && labelPos != KnobLabelPos::None;
    ImVec2      lblSz    = {0.f, 0.f};
    if (hasLabel) {
        if (g_fontKnobLabel) ImGui::PushFont(g_fontKnobLabel);
        lblSz = ImGui::CalcTextSize(label);
        if (g_fontKnobLabel) ImGui::PopFont();
    }

    float totalW, lcdX, lcdY;
    knobCalcLayout(lblSz, hasLabel, totalW, lcdX, lcdY);

    bool   editing = (s_activeID == wid);
    ImVec2 sp      = ImGui::GetCursorScreenPos();

    bool editReq = false;
    bool changed = KnobWidget(id, value, vmin, vmax, &editReq, editing, bipolar, label, labelPos);

    if (editReq) {
        s_activeID = wid;
        s_editVal  = bipolar ? (*value - (vmin + vmax) / 2) : *value;
        s_doFocus  = true;
    }

    if (editing) {
        // InputText overlaid on the LCD housing
        ImGui::SetCursorScreenPos({sp.x + lcdX, sp.y + lcdY});
        ImGui::SetNextItemWidth(KNOB_LCD_W);
        if (s_doFocus) { ImGui::SetKeyboardFocusHere(); s_doFocus = false; }

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text,           kClrLcdText);
        ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_NavHighlight,   IM_COL32(0,0,0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    ImVec2(0.f, 0.f));

        ImGui::PushID(id);
        if (ImGui::InputInt("##ke", &s_editVal, 0, 0,
                ImGuiInputTextFlags_EnterReturnsTrue |
                ImGuiInputTextFlags_AutoSelectAll)) {
            int finalVal = bipolar ? ((vmin + vmax) / 2 + s_editVal) : s_editVal;
            *value     = std::clamp(finalVal, vmin, vmax);
            changed    = true;
            s_activeID = 0;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            s_activeID = 0;
        ImGui::PopID();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(6);

        ImGui::SetCursorScreenPos(sp);
        ImGui::Dummy({totalW, KNOB_BTN_H});
    }

    return changed;
}
