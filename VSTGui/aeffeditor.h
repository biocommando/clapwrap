// Compatibility wrapper for FST plugins that use VSTGUI library.
#pragma once
#include "windows.h"
#include "guibase.h"

class AEffEditor : public GuiBase
{
protected:
    void *fstEffect;
    void *systemWindow = nullptr;

public:
    AEffEditor(void *effect) : fstEffect(effect) {}
    virtual void idle() {}

    virtual ~AEffEditor() {}
};