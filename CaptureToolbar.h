#pragma once

#include <windows.h>

constexpr UINT kCaptureToolbarCopyMessage = WM_APP + 100;
constexpr UINT kCaptureToolbarSaveMessage = WM_APP + 101;
constexpr UINT kCaptureToolbarCancelMessage = WM_APP + 102;

class CaptureToolbar
{
public:
    CaptureToolbar() = default;
    ~CaptureToolbar();

    CaptureToolbar(const CaptureToolbar&) = delete;
    CaptureToolbar& operator=(const CaptureToolbar&) = delete;

    bool Show(HWND owner, POINT anchorScreenPosition);
    void Hide();

private:
    HWND hwnd_ = nullptr;
};
