#include "SettingsDialog.h"

#include "Settings.h"
#include "StartupManager.h"

#include <array>
#include <string>
#include <utility>

namespace
{
constexpr wchar_t kSettingsDialogClassName[] = L"PrtScSettingsDialog";
constexpr int kEditHotkeyId = 2001;
constexpr int kRunAtStartupCheckboxId = 2002;
constexpr int kButtonOkId = IDOK;
constexpr int kButtonCancelId = IDCANCEL;
constexpr int kBaseDpi = 96;

struct DialogState
{
    HWND editHotkey = nullptr;
    HWND runAtStartupCheckbox = nullptr;
    WNDPROC originalEditProc = nullptr;
    bool accepted = false;
    bool closed = false;
};

HFONT GetDefaultGuiFont()
{
    return reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

void ApplyDefaultFont(HWND hwnd)
{
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetDefaultGuiFont()), TRUE);
}

UINT GetWindowDpi(HWND hwnd)
{
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto getDpiForWindow = user32 != nullptr
        ? reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"))
        : nullptr;

    if (getDpiForWindow != nullptr)
    {
        return getDpiForWindow(hwnd);
    }

    HDC screenDc = GetDC(hwnd);
    if (screenDc == nullptr)
    {
        return kBaseDpi;
    }

    const int dpi = GetDeviceCaps(screenDc, LOGPIXELSX);
    ReleaseDC(hwnd, screenDc);
    return dpi > 0 ? static_cast<UINT>(dpi) : kBaseDpi;
}

int ScaleForDpi(HWND hwnd, int value)
{
    const UINT dpi = GetWindowDpi(hwnd);
    return MulDiv(value, static_cast<int>(dpi), kBaseDpi);
}

std::wstring GetWindowTextValue(HWND hwnd)
{
    const int length = GetWindowTextLengthW(hwnd);
    std::wstring value(static_cast<size_t>(length + 1), L'\0');
    GetWindowTextW(hwnd, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    return value;
}

bool IsKeyPressed(int virtualKey)
{
    return (GetKeyState(virtualKey) & 0x8000) != 0;
}

bool IsModifierKey(WPARAM virtualKey)
{
    switch (virtualKey)
    {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LWIN:
    case VK_RWIN:
        return true;

    default:
        return false;
    }
}

std::wstring GetKeyName(WPARAM virtualKey, LPARAM lparam)
{
    switch (virtualKey)
    {
    case VK_SNAPSHOT:
        return L"PrtScr";
    case VK_ESCAPE:
        return L"Esc";
    case VK_RETURN:
        return L"Enter";
    case VK_SPACE:
        return L"Space";
    case VK_TAB:
        return L"Tab";
    case VK_BACK:
        return L"Backspace";
    case VK_DELETE:
        return L"Delete";
    case VK_INSERT:
        return L"Insert";
    case VK_HOME:
        return L"Home";
    case VK_END:
        return L"End";
    case VK_PRIOR:
        return L"PageUp";
    case VK_NEXT:
        return L"PageDown";
    case VK_LEFT:
        return L"Left";
    case VK_RIGHT:
        return L"Right";
    case VK_UP:
        return L"Up";
    case VK_DOWN:
        return L"Down";
    default:
        break;
    }

    wchar_t name[64]{};
    LONG scanCode = static_cast<LONG>((lparam >> 16) & 0xFF);
    if ((lparam & (1LL << 24)) != 0)
    {
        scanCode |= 0x100;
    }

    if (GetKeyNameTextW(scanCode << 16, name, static_cast<int>(sizeof(name) / sizeof(name[0]))) > 0)
    {
        return name;
    }

    return L"";
}

std::wstring BuildHotkeyText(WPARAM virtualKey, LPARAM lparam)
{
    if (IsModifierKey(virtualKey))
    {
        return {};
    }

    std::wstring keyName = GetKeyName(virtualKey, lparam);
    if (keyName.empty())
    {
        return {};
    }

    std::wstring result;
    const std::array<std::pair<int, const wchar_t*>, 4> modifiers{{
        {VK_CONTROL, L"Ctrl"},
        {VK_MENU, L"Alt"},
        {VK_SHIFT, L"Shift"},
        {VK_LWIN, L"Win"},
    }};

    for (const auto& [key, name] : modifiers)
    {
        const bool isWin = key == VK_LWIN;
        const bool pressed = isWin ? (IsKeyPressed(VK_LWIN) || IsKeyPressed(VK_RWIN)) : IsKeyPressed(key);
        if (!pressed)
        {
            continue;
        }

        if (!result.empty())
        {
            result += L"+";
        }
        result += name;
    }

    if (!result.empty())
    {
        result += L"+";
    }
    result += keyName;
    return result;
}

LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTCHARS;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || wparam == VK_SNAPSHOT)
        {
            const std::wstring hotkey = BuildHotkeyText(wparam, lparam);
            if (!hotkey.empty())
            {
                SetWindowTextW(hwnd, hotkey.c_str());
                SendMessageW(hwnd, EM_SETSEL, 0, -1);
            }
        }
        return 0;

    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_PASTE:
    case WM_CUT:
    case WM_CLEAR:
        return 0;

    default:
        break;
    }

    if (state != nullptr && state->originalEditProc != nullptr)
    {
        return CallWindowProcW(state->originalEditProc, hwnd, message, wparam, lparam);
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void CloseDialog(HWND hwnd, bool accepted)
{
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state != nullptr)
    {
        state->accepted = accepted;
        state->closed = true;
    }

    DestroyWindow(hwnd);
}

LRESULT CALLBACK SettingsDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        auto* state = reinterpret_cast<DialogState*>(reinterpret_cast<LPCREATESTRUCTW>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        const int margin = ScaleForDpi(hwnd, 22);
        const int labelTop = ScaleForDpi(hwnd, 22);
        const int editTop = ScaleForDpi(hwnd, 54);
        const int checkboxTop = ScaleForDpi(hwnd, 98);
        const int buttonTop = ScaleForDpi(hwnd, 145);
        const int editWidth = ScaleForDpi(hwnd, 390);
        const int labelHeight = ScaleForDpi(hwnd, 22);
        const int editHeight = ScaleForDpi(hwnd, 30);
        const int checkboxHeight = ScaleForDpi(hwnd, 26);
        const int buttonWidth = ScaleForDpi(hwnd, 96);
        const int buttonHeight = ScaleForDpi(hwnd, 34);
        const int buttonGap = ScaleForDpi(hwnd, 10);

        HWND label = CreateWindowExW(
            0,
            L"STATIC",
            L"Screenshot hotkey:",
            WS_CHILD | WS_VISIBLE,
            margin,
            labelTop,
            editWidth,
            labelHeight,
            hwnd,
            nullptr,
            nullptr,
            nullptr);
        ApplyDefaultFont(label);

        state->editHotkey = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            Settings::Instance().ScreenshotHotkey().c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_READONLY,
            margin,
            editTop,
            editWidth,
            editHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditHotkeyId)),
            nullptr,
            nullptr);
        ApplyDefaultFont(state->editHotkey);
        SetWindowLongPtrW(state->editHotkey, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->originalEditProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(state->editHotkey, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HotkeyEditProc)));

        state->runAtStartupCheckbox = CreateWindowExW(
            0,
            L"BUTTON",
            L"Run at system startup",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            margin,
            checkboxTop,
            editWidth,
            checkboxHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRunAtStartupCheckboxId)),
            nullptr,
            nullptr);
        ApplyDefaultFont(state->runAtStartupCheckbox);
        SendMessageW(
            state->runAtStartupCheckbox,
            BM_SETCHECK,
            Settings::Instance().RunAtSystemStartup() ? BST_CHECKED : BST_UNCHECKED,
            0);

        HWND okButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            margin + editWidth - (buttonWidth * 2) - buttonGap,
            buttonTop,
            buttonWidth,
            buttonHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kButtonOkId)),
            nullptr,
            nullptr);
        ApplyDefaultFont(okButton);

        HWND cancelButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Cancel",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            margin + editWidth - buttonWidth,
            buttonTop,
            buttonWidth,
            buttonHeight,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kButtonCancelId)),
            nullptr,
            nullptr);
        ApplyDefaultFont(cancelButton);

        SetFocus(state->editHotkey);
        SendMessageW(state->editHotkey, EM_SETSEL, 0, -1);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case kButtonOkId:
        {
            auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (state != nullptr)
            {
                Settings::Instance().SetScreenshotHotkey(GetWindowTextValue(state->editHotkey));
                const bool runAtStartup =
                    SendMessageW(state->runAtStartupCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
                Settings::Instance().SetRunAtSystemStartup(runAtStartup);
                Settings::Instance().Save();
                SetRunAtSystemStartup(runAtStartup);
            }

            CloseDialog(hwnd, true);
            return 0;
        }

        case kButtonCancelId:
            CloseDialog(hwnd, false);
            return 0;

        default:
            break;
        }
        break;

    case WM_CLOSE:
        CloseDialog(hwnd, false);
        return 0;

    case WM_DESTROY:
    {
        auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (state != nullptr)
        {
            state->closed = true;
        }
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool RegisterSettingsDialogClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = SettingsDialogProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = kSettingsDialogClassName;

    if (RegisterClassExW(&windowClass) != 0)
    {
        return true;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
}

bool ShowSettingsDialog(HWND owner)
{
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    if (!RegisterSettingsDialogClass(instance))
    {
        return false;
    }

    DialogState state;
    const UINT dpi = GetWindowDpi(owner);
    const int dialogWidth = MulDiv(460, static_cast<int>(dpi), kBaseDpi);
    const int dialogHeight = MulDiv(250, static_cast<int>(dpi), kBaseDpi);
    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kSettingsDialogClassName,
        L"Settings",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        dialogWidth,
        dialogHeight,
        owner,
        nullptr,
        instance,
        &state);

    if (dialog == nullptr)
    {
        return false;
    }

    RECT dialogRect{};
    GetWindowRect(dialog, &dialogRect);

    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);

    const int actualDialogWidth = dialogRect.right - dialogRect.left;
    const int actualDialogHeight = dialogRect.bottom - dialogRect.top;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - actualDialogWidth) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - actualDialogHeight) / 2;
    SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    MSG message{};
    while (!state.closed && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(dialog, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return state.accepted;
}
