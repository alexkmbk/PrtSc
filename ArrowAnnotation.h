#pragma once

#include <windows.h>

class ArrowAnnotation
{
public:
    ArrowAnnotation() = default;
    ~ArrowAnnotation();

    ArrowAnnotation(const ArrowAnnotation&) = delete;
    ArrowAnnotation& operator=(const ArrowAnnotation&) = delete;

    bool Show(HWND owner, POINT screenStart, POINT screenEnd, COLORREF color);
    void Hide();

private:
    HWND hwnd_ = nullptr;
};
