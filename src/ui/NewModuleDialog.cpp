#include "NewModuleDialog.h"
#include "Theme.h"
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
    if (!m_popup.begin("New Module", 280.f, 160.f))
        return;

    ImGui::PushStyleColor(ImGuiCol_Button,        kClrBtnBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kClrBtnHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kClrBtnActive);

    const float btnW = 256.f;
    int chosen = -1;

    ImGui::Spacing();
    for (int i = 0; i < kNumFormats; ++i) {
        ImGui::SetCursorPosX(12.f);
        if (ImGui::Button(kFormats[i].label, {btnW, 0.f}))
            chosen = i;
    }

    ImGui::PopStyleColor(3);

    if (chosen >= 0)
        ImGui::CloseCurrentPopup();

    m_popup.end();

    if (chosen >= 0 && onCreate)
        onCreate(chosen);
}
