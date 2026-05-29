#include "NewModuleDialog.h"
#include "imgui.h"

namespace {
    struct FormatEntry { const char* label; const char* ext; };
    constexpr FormatEntry kFormats[] = {
        { "BerryBeatz  (.btz)",       ".btz" },
        { "Impulse Tracker  (.it)",   ".it"  },
        { "FastTracker II  (.xm)",    ".xm"  },
        { "Scream Tracker 3  (.s3m)", ".s3m" },
        { "ProTracker  (.mod)",       ".mod" },
    };
    constexpr int kNumFormats = 5;
}

void NewModuleDialog::open()
{
    m_popup.open();
}

void NewModuleDialog::draw()
{
    constexpr float kW = 300.f;
    constexpr float kH = 192.f;
    if (!m_popup.begin("New Module", kW, kH))
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.f);
    ImGui::SetCursorPosX(12.f);
    ImGui::TextUnformatted("Select format:");
    ImGui::Spacing();

    int chosen = -1;
    for (int i = 0; i < kNumFormats; ++i) {
        ImGui::SetCursorPosX(12.f);
        if (ImGui::Button(kFormats[i].label, {kW - 24.f, 0.f}))
            chosen = i;
        if (i < kNumFormats - 1) ImGui::Spacing();
    }

    if (chosen >= 0)
        ImGui::CloseCurrentPopup();

    m_popup.end();

    if (chosen >= 0 && onCreate)
        onCreate(chosen);
}
