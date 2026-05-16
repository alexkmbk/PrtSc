#pragma once

#include <windows.h>

constexpr UINT kCaptureToolbarCopyMessage = WM_APP + 100;
constexpr UINT kCaptureToolbarSaveMessage = WM_APP + 101;
constexpr UINT kCaptureToolbarCancelMessage = WM_APP + 102;
constexpr UINT kCaptureToolbarColorMessage = WM_APP + 103;
constexpr UINT kCaptureToolbarArrowMessage = WM_APP + 104;

class CaptureToolbar
{
public:
    CaptureToolbar() = default;
    ~CaptureToolbar();

    CaptureToolbar(const CaptureToolbar&) = delete;
    CaptureToolbar& operator=(const CaptureToolbar&) = delete;

    bool Show(HWND owner, POINT anchorScreenPosition, COLORREF color);
    void Hide();
    void SetColor(COLORREF color);
    void AttachColorButton(HWND button);
    void AttachArrowButton(HWND button);
    COLORREF Color() const;

private:
    HWND hwnd_ = nullptr;
    HWND colorButton_ = nullptr;
    HWND arrowButton_ = nullptr;
    COLORREF color_ = RGB(255, 0, 0);
};
