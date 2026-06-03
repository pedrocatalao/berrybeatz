#pragma once
#include "ModuleState.h"
#include "ModuleController.h"

struct ImFont;

class PropertiesView {
public:
    explicit PropertiesView(ModuleState& state);
    void setController(ModuleController* ctrl);
    void clearModule();
    void setFonts(ImFont* mono, ImFont* ui);
    void render(float x, float y, float w, float h);

private:
    ModuleState&       m_state;
    ModuleController*  m_ctrl     {nullptr};
    ImFont*            m_fontMono {nullptr};
    ImFont*            m_fontUI   {nullptr};
};
