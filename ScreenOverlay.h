#pragma once

#include <windows.h>

class ScreenOverlay
{
public:
    bool Show(HINSTANCE instance);
    void Close();

private:
    HWND hwnd_ = nullptr;
};
