#include "SysTrayIcon.h"

#include "AppMessages.h"
#include "resource.h"
#include "SettingsDialog.h"

namespace
{
constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayIconMessage = WM_APP + 1;
constexpr UINT kMenuTakeScreenshotId = 1000;
constexpr UINT kMenuSettingsId = 1001;
constexpr UINT kMenuExitId = 1002;
constexpr wchar_t kTrayTooltip[] = L"PrtSc";
}

SysTrayIcon::~SysTrayIcon()
{
    Shutdown();
}

bool SysTrayIcon::Initialize(HWND hwnd)
{
    if (isInitialized_)
    {
        return true;
    }

    notifyIconData_ = {};
    notifyIconData_.cbSize = sizeof(notifyIconData_);
    notifyIconData_.hWnd = hwnd;
    notifyIconData_.uID = kTrayIconId;
    notifyIconData_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    notifyIconData_.uCallbackMessage = kTrayIconMessage;
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
    icon_ = reinterpret_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_PRTSC_APP),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR));
    notifyIconData_.hIcon = icon_ != nullptr ? icon_ : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(notifyIconData_.szTip, kTrayTooltip);

    isInitialized_ = Shell_NotifyIconW(NIM_ADD, &notifyIconData_) != FALSE;
    if (!isInitialized_ && icon_ != nullptr)
    {
        DestroyIcon(icon_);
        icon_ = nullptr;
    }
    return isInitialized_;
}

void SysTrayIcon::Shutdown()
{
    if (!isInitialized_)
    {
        return;
    }

    Shell_NotifyIconW(NIM_DELETE, &notifyIconData_);
    if (icon_ != nullptr)
    {
        DestroyIcon(icon_);
        icon_ = nullptr;
    }
    isInitialized_ = false;
}

bool SysTrayIcon::HandleMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == kTrayIconMessage && wparam == kTrayIconId)
    {
        if (LOWORD(lparam) == WM_LBUTTONUP || LOWORD(lparam) == NIN_SELECT || LOWORD(lparam) == WM_RBUTTONUP ||
            LOWORD(lparam) == WM_CONTEXTMENU)
        {
            ShowContextMenu(hwnd);
            return true;
        }

        return false;
    }

    if (message == WM_COMMAND)
    {
        switch (LOWORD(wparam))
        {
        case kMenuTakeScreenshotId:
            PostMessageW(hwnd, kAppTakeScreenshotMessage, 0, 0);
            return true;

        case kMenuSettingsId:
            if (ShowSettingsDialog(hwnd))
            {
                PostMessageW(hwnd, kAppSettingsChangedMessage, 0, 0);
            }
            return true;

        case kMenuExitId:
            DestroyWindow(hwnd);
            return true;

        default:
            return false;
        }
    }

    return false;
}

void SysTrayIcon::ShowContextMenu(HWND hwnd) const
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING, kMenuTakeScreenshotId, L"Take a screenshot");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuSettingsId, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExitId, L"Exit");

    POINT cursorPosition{};
    GetCursorPos(&cursorPosition);

    SetForegroundWindow(hwnd);
    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        cursorPosition.x,
        cursorPosition.y,
        0,
        hwnd,
        nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    DestroyMenu(menu);
}
