#pragma once
#include "imgui.h"
#include "Theme.h"

// ---------------------------------------------------------------------------
// Reusable styled popup: drop-shadow, gradient bg, custom title bar,
// drag-to-move, × close button, no dim backdrop, no click-through.
//
// Usage (every frame inside the host window):
//   if (m_popup.begin("Title", 500, 340)) {
//       // draw content — cursor is already below the title bar
//       m_popup.end();
//   }
//
// To open from anywhere (same window context):
//   m_popup.open();   // resets position to screen centre
// ---------------------------------------------------------------------------
class PopupModal {
public:
    static constexpr float kTitleH = 22.f;

    void open()
    {
        m_open       = true;
        m_shouldOpen = true;
        m_winPosX    = -1.f;
        m_winPosY    = -1.f;
    }

    // Returns true if content should be drawn. Must call end() iff true.
    bool begin(const char* title, float w, float h)
    {
        if (!m_open && !m_shouldOpen) return false;

        if (m_shouldOpen) {
            ImGui::OpenPopup(title);
            m_shouldOpen = false;
        }

        if (m_winPosX < 0.f) {
            ImVec2 c  = ImGui::GetMainViewport()->GetCenter();
            m_winPosX = c.x - w * 0.5f;
            m_winPosY = c.y - h * 0.5f;
        }

        ImGui::SetNextWindowPos({m_winPosX, m_winPosY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({w, h},                ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_PopupBg,          kClrModalBg);

        bool visible = ImGui::BeginPopupModal(title, &m_open,
            ImGuiWindowFlags_NoTitleBar        |
            ImGuiWindowFlags_NoResize          |
            ImGuiWindowFlags_NoScrollbar       |
            ImGuiWindowFlags_NoScrollWithMouse);

        if (!visible) {
            ImGui::PopStyleColor(2);
            if (!ImGui::IsPopupOpen(title)) m_open = false;
            return false;
        }

        drawChrome(title, w, h);
        return true;
    }

    // Must be called iff begin() returned true.
    void end()
    {
        ImGui::EndPopup();
        ImGui::PopStyleColor(2);
    }

    bool isOpen() const { return m_open; }

private:
    void drawChrome(const char* title, float w, float h)
    {
        ImVec2      p0  = ImGui::GetWindowPos();
        ImVec2      p1  = {p0.x + w, p0.y + h};
        ImDrawList* wdl = ImGui::GetWindowDrawList();

        // Drop shadow (bleeds outside window bounds)
        wdl->PushClipRectFullScreen();
        for (int i = 24; i >= 1; i--) {
            float   f = float(i) / 24.f;
            float   r = 40.f * f;
            uint8_t a = (uint8_t)(10.f * (1.f - f) * (1.f - f));
            if (!a) continue;
            wdl->AddRectFilled({p0.x - r, p0.y + 10.f - r},
                               {p1.x + r, p1.y +  2.f + r},
                               IM_COL32(0, 0, 0, a), 6.f + r * 0.5f);
        }
        wdl->PopClipRect();

        // Gradient background
        wdl->AddRectFilledMultiColor(p0, p1,
            kClrModalGradTop, kClrModalGradTop,
            kClrModalGradBot, kClrModalGradBot);

        // Title bar
        wdl->AddRectFilled(p0, {p1.x, p0.y + kTitleH}, kClrBgTitle);
        {
            ImVec2 tsz = ImGui::CalcTextSize(title);
            wdl->AddText({p0.x + (w - tsz.x) * 0.5f, p0.y + (kTitleH - tsz.y) * 0.5f},
                         kClrTextTitle, title);
        }

        // Drag handle (full title bar minus × button)
        ImGui::SetCursorPos({0.f, 0.f});
        ImGui::InvisibleButton("##drag", {w - kTitleH, kTitleH});
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f)) {
            ImVec2 d  = ImGui::GetIO().MouseDelta;
            m_winPosX += d.x;
            m_winPosY += d.y;
        }

        // × close button — same size/style as the main window controls.
        // Main window uses GetFrameHeight() under kToolbarFramePad {8,2},
        // which evaluates to FontSize + 2*2 = 17px. Derive it the same way
        // and centre it in the title bar.
        {
            const float bsz    = ImGui::GetTextLineHeight() + 4.f; // matches toolbar's 17px
            const float margin = (float)(int)((kTitleH - bsz) * 0.5f);
            ImGui::SetCursorPos({w - kTitleH + margin, margin});
            ImVec2 bp = ImGui::GetCursorScreenPos();
            bool clicked = ImGui::InvisibleButton("##close", {bsz, bsz});
            bool hov = ImGui::IsItemHovered();
            wdl->AddRectFilled(bp, {bp.x + bsz, bp.y + bsz},
                               hov ? IM_COL32(140, 28, 28, 255) : IM_COL32(38, 38, 58, 255));
            const float m  = 4.f;
            const ImU32 sc = IM_COL32(210, 210, 220, 255);
            wdl->AddLine({bp.x + m,       bp.y + m},       {bp.x + bsz - m, bp.y + bsz - m}, sc, 1.5f);
            wdl->AddLine({bp.x + bsz - m, bp.y + m},       {bp.x + m,       bp.y + bsz - m}, sc, 1.5f);
            if (clicked) {
                m_open = false;
                ImGui::CloseCurrentPopup();
            }
        }

        // Position cursor below title bar for content
        ImGui::SetCursorPos({0.f, kTitleH});
    }

    bool  m_open       {false};
    bool  m_shouldOpen {false};
    float m_winPosX    {-1.f};
    float m_winPosY    {-1.f};
};
