#include "CaptureToolbar.h"

#include <algorithm>

namespace
{
constexpr wchar_t kToolbarClassName[] = L"PrtScCaptureToolbar";
constexpr int kToolbarWidth = 330;
constexpr int kToolbarHeight = 58;
constexpr int kToolbarMargin = 8;
constexpr int kButtonWidth = 96;
constexpr int kButtonHeight = 36;
constexpr int kButtonTop = 10;
constexpr int kCopyButtonId = 3001;
constexpr int kSaveButtonId = 3002;
constexpr int kCancelButtonId = 3003;

HFONT GetDefaultGuiFont()
{
    return reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

void ApplyDefaultFont(HWND hwnd)
{
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetDefaultGuiFont()), TRUE);
}

void PlaceBelowSelection(HWND hwnd, POINT anchor)
{
    const int screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int screenRight = screenLeft + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int screenBottom = screenTop + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    int x = anchor.x - kToolbarWidth;
    int y = anchor.y + kToolbarMargin;

    if (y + kToolbarHeight > screenBottom)
    {
        y = anchor.y - kToolbarHeight - kToolbarMargin;
    }

    x = std::max(screenLeft, std::min(x, screenRight - kToolbarWidth));
    y = std::max(screenTop, std::min(y, screenBottom - kToolbarHeight));

    SetWindowPos(hwnd, HWND_TOPMOST, x, y, kToolbarWidth, kToolbarHeight, SWP_SHOWWINDOW);
}

HWND CreateToolbarButton(HWND parent, int id, const wchar_t* text, int left)
{
    HWND button = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        left,
        kButtonTop,
        kButtonWidth,
        kButtonHeight,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        nullptr,
        nullptr);
    ApplyDefaultFont(button);
    return button;
}

LRESULT CALLBACK ToolbarProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateToolbarButton(hwnd, kCopyButtonId, L"Copy", 12);
        CreateToolbarButton(hwnd, kSaveButtonId, L"Save", 117);
        CreateToolbarButton(hwnd, kCancelButtonId, L"Cancel", 222);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            if (HWND owner = GetWindow(hwnd, GW_OWNER))
            {
                PostMessageW(owner, kCaptureToolbarCancelMessage, 0, 0);
            }
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case kCancelButtonId:
            if (HWND owner = GetWindow(hwnd, GW_OWNER))
            {
                PostMessageW(owner, kCaptureToolbarCancelMessage, 0, 0);
            }
            return 0;

        case kCopyButtonId:
            if (HWND owner = GetWindow(hwnd, GW_OWNER))
            {
                PostMessageW(owner, kCaptureToolbarCopyMessage, 0, 0);
            }
            return 0;

        case kSaveButtonId:
            if (HWND owner = GetWindow(hwnd, GW_OWNER))
            {
                PostMessageW(owner, kCaptureToolbarSaveMessage, 0, 0);
            }
            return 0;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool RegisterToolbarClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ToolbarProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = kToolbarClassName;

    if (RegisterClassExW(&windowClass) != 0)
    {
        return true;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
}

CaptureToolbar::~CaptureToolbar()
{
    Hide();
}

bool CaptureToolbar::Show(HWND owner, POINT anchorScreenPosition)
{
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    if (!RegisterToolbarClass(instance))
    {
        return false;
    }

    if (hwnd_ == nullptr || !IsWindow(hwnd_))
    {
        hwnd_ = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            kToolbarClassName,
            L"PrtSc Capture Toolbar",
            WS_POPUP | WS_BORDER,
            anchorScreenPosition.x,
            anchorScreenPosition.y,
            kToolbarWidth,
            kToolbarHeight,
            owner,
            nullptr,
            instance,
            nullptr);

        if (hwnd_ == nullptr)
        {
            return false;
        }
    }

    PlaceBelowSelection(hwnd_, anchorScreenPosition);
    return true;
}

void CaptureToolbar::Hide()
{
    if (hwnd_ != nullptr && IsWindow(hwnd_))
    {
        DestroyWindow(hwnd_);
    }

    hwnd_ = nullptr;
}
