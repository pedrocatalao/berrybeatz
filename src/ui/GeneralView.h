#pragma once
#include "ModuleState.h"

namespace openmpt { class module_ext; }
struct ImFont;

class GeneralView {
public:
    explicit GeneralView(ModuleState& state);
    void setModule(openmpt::module_ext* mod);
    void clearModule();
    void setFonts(ImFont* mono, ImFont* ui);
    void render(float x, float y, float w, float h);

private:
    ModuleState&          m_state;
    openmpt::module_ext*  m_mod      {nullptr};
    ImFont*               m_fontMono {nullptr};
    ImFont*               m_fontUI   {nullptr};
};
