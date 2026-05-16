#include "AppMessages.h"
#include "CaptureToolbar.h"
#include "HotkeyManager.h"
#include "resource.h"
#include "ScreenOverlay.h"
#include "SysTrayIcon.h"

#include "Settings.h"

#include <windows.h>

namespace
{
constexpr wchar_t kWindowClassName[] = L"PrtScWindowClass";
constexpr wchar_t kWindowTitle[] = L"PrtSc";
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\PrtScSingleInstance";

SysTrayIcon gSysTrayIcon;
HotkeyManager gHotkeyManager;
ScreenOverlay gScreenOverlay;
HANDLE gSingleInstanceMutex = nullptr;

void EnableDpiAwareness()
{
    using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto setProcessDpiAwarenessContext = user32 != nullptr
        ? reinterpret_cast<SetProcessDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"))
        : nullptr;

    if (setProcessDpiAwarenessContext != nullptr &&
        setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
    {
        return;
    }

    SetProcessDPIAware();
}

bool EnsureSingleInstance()
{
    gSingleInstanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
    return gSingleInstanceMutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (gSysTrayIcon.HandleMessage(hwnd, message, wparam, lparam))
    {
        return 0;
    }

    switch (message)
    {
    case WM_CREATE:
        if (!gSysTrayIcon.Initialize(hwnd))
        {
            return -1;
        }
        gHotkeyManager.Register(hwnd);
        return 0;

    case kAppSettingsChangedMessage:
        gHotkeyManager.Register(hwnd);
        return 0;

    case kAppTakeScreenshotMessage:
    {
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
        gScreenOverlay.Show(instance);
        return 0;
    }

    case WM_HOTKEY:
        if (gHotkeyManager.IsHotkeyMessage(message, wparam))
        {
            HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            gScreenOverlay.Show(instance);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        gScreenOverlay.Close();
        gHotkeyManager.Unregister(hwnd);
        gSysTrayIcon.Shutdown();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

bool RegisterMainWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_PRTSC_APP));
    windowClass.hIconSm = reinterpret_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_PRTSC_APP),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    return RegisterClassExW(&windowClass) != 0;
}

HWND CreateMainWindow(HINSTANCE instance, int showCommand)
{
    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        600,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr)
    {
        return nullptr;
    }

    return hwnd;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    EnableDpiAwareness();

    if (!EnsureSingleInstance())
    {
        return 0;
    }

    Settings::Instance().Load();
    InitializeCaptureToolbarOcrSupport();

    if (!RegisterMainWindowClass(instance))
    {
        return 1;
    }

    if (CreateMainWindow(instance, showCommand) == nullptr)
    {
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (gSingleInstanceMutex != nullptr)
    {
        CloseHandle(gSingleInstanceMutex);
        gSingleInstanceMutex = nullptr;
    }

    return static_cast<int>(message.wParam);
}
