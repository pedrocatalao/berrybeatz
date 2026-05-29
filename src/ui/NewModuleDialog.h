#pragma once
#include "PopupModal.h"
#include <functional>

class NewModuleDialog {
public:
    // Called with format index: 0=btz, 1=it, 2=xm, 3=s3m, 4=mod
    std::function<void(int format)> onCreate;

    void open();   // trigger; resets selection to default (.btz)
    void draw();   // call every frame

private:
    PopupModal m_popup;
};
