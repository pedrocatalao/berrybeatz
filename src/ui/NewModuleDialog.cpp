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
    m_format = 0;
    m_popup.open();
}

void NewModuleDialog::draw()
{
    if (!m_popup.begin("New Module", 320.f, 218.f))
        return;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.f);
    ImGui::SetCursorPosX(12.f);
    ImGui::TextUnformatted("Select format:");

    ImGui::Spacing();
    for (int i = 0; i < kNumFormats; ++i) {
        ImGui::SetCursorPosX(12.f);
        ImGui::RadioButton(kFormats[i].label, &m_format, i);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetCursorPosX(12.f);
    bool create = ImGui::Button("Create", ImVec2(136.f, 0));
    ImGui::SameLine(0.f, 8.f);
    bool cancel = ImGui::Button("Cancel", ImVec2(136.f, 0));

    if (create || cancel)
        ImGui::CloseCurrentPopup();

    m_popup.end();

    if (create && onCreate)
        onCreate(m_format);
}
