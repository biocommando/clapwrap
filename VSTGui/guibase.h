// Compatibility wrapper for FST plugins that use VSTGUI library.
#pragma once

struct ERect {
    short top;
    short left;
    short bottom;
    short right;
};

class GuiBase
{
public:
    virtual bool open(void *ptr) { return false; }
    virtual void close() {}
    virtual bool getRect(ERect **ppRect) { return false; }
    virtual void draw(ERect *pRect) {}
    virtual bool getFrameDirtyStatus() { return false; }
    virtual ~GuiBase() {}
};