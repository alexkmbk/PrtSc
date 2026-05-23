#include "CaptureToolbar.h"

#include <algorithm>
#include <commctrl.h>

namespace
{
constexpr wchar_t kToolbarClassName[] = L"PrtScCaptureToolbar";
constexpr int kToolbarHeight = 58;
constexpr int kToolbarMargin = 8;
constexpr int kCommandButtonWidth = 104;
constexpr int kButtonHeight = 36;
constexpr int kToolButtonWidth = kButtonHeight;
constexpr int kButtonTop = 10;
constexpr int kButtonLeft = 12;
constexpr int kButtonRight = kButtonLeft;
constexpr int kButtonGap = 8;
constexpr int kSeparatorWidth = 1;
constexpr int kSeparatorHeight = 32;
constexpr int kSeparatorTop = 13;
constexpr int kSeparatorGap = 14;
constexpr int kSeparatorSpace = (kSeparatorGap * 2) + kSeparatorWidth;
constexpr int kCopyButtonId = 3001;
constexpr int kSaveButtonId = 3002;
constexpr int kCancelButtonId = 3003;
constexpr int kColorButtonId = 3004;
constexpr int kArrowButtonId = 3005;
constexpr int kOcrButtonId = 3006;
constexpr int kTextButtonId = 3007;
constexpr UINT kTooltipInfoSize = TTTOOLINFOW_V1_SIZE;

bool gIsOcrSupported = false;

HFONT GetDefaultGuiFont()
{
    return reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

void ApplyDefaultFont(HWND hwnd)
{
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetDefaultGuiFont()), TRUE);
}

bool IsOcrSupported();

int GetToolbarWidth()
{
    const int commandButtonCount = IsOcrSupported() ? 4 : 3;
    constexpr int toolButtonCount = 3;
    const int buttonCount = commandButtonCount + toolButtonCount;
    return kButtonLeft + kButtonRight + (commandButtonCount * kCommandButtonWidth) +
        (toolButtonCount * kToolButtonWidth) + ((buttonCount - 3) * kButtonGap) + (2 * kSeparatorSpace);
}

void PlaceBelowSelection(HWND hwnd, POINT anchor)
{
    const int screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int screenRight = screenLeft + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int screenBottom = screenTop + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const int toolbarWidth = GetToolbarWidth();

    int x = anchor.x - toolbarWidth;
    int y = anchor.y + kToolbarMargin;

    if (y + kToolbarHeight > screenBottom)
    {
        y = anchor.y - kToolbarHeight - kToolbarMargin;
    }

    x = std::max(screenLeft, std::min(x, screenRight - toolbarWidth));
    y = std::max(screenTop, std::min(y, screenBottom - kToolbarHeight));

    SetWindowPos(hwnd, HWND_TOPMOST, x, y, toolbarWidth, kToolbarHeight, SWP_SHOWWINDOW);
}

CaptureToolbar* GetToolbar(HWND hwnd)
{
    return reinterpret_cast<CaptureToolbar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

bool IsCtrlPressed()
{
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

void PostOwnerMessage(HWND hwnd, UINT message)
{
    if (HWND owner = GetWindow(hwnd, GW_OWNER))
    {
        PostMessageW(owner, message, 0, 0);
    }
}

void PostOwnerMessageFromChild(HWND child, UINT message)
{
    HWND toolbar = GetParent(child);
    if (toolbar != nullptr)
    {
        PostOwnerMessage(toolbar, message);
    }
}

bool DetectOcrSupport()
{
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr)
    {
        return false;
    }

    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtlGetVersion == nullptr)
    {
        return false;
    }

    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtlGetVersion(&version) != 0)
    {
        return false;
    }

    return version.dwMajorVersion >= 10;
}

bool IsOcrSupported()
{
    return gIsOcrSupported;
}

LRESULT CALLBACK ToolbarButtonProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
{
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
    {
        if (wparam == VK_ESCAPE)
        {
            PostOwnerMessageFromChild(hwnd, kCaptureToolbarCancelMessage);
            return 0;
        }

        if (IsCtrlPressed())
        {
            if (wparam == 'C')
            {
                PostOwnerMessageFromChild(hwnd, kCaptureToolbarCopyMessage);
                return 0;
            }
            if (wparam == 'S')
            {
                PostOwnerMessageFromChild(hwnd, kCaptureToolbarSaveMessage);
                return 0;
            }
            if (wparam == 'O')
            {
                if (IsOcrSupported())
                {
                    PostOwnerMessageFromChild(hwnd, kCaptureToolbarOcrMessage);
                }
                return 0;
            }
            if (wparam == 'T')
            {
                PostOwnerMessageFromChild(hwnd, kCaptureToolbarTextMessage);
                return 0;
            }
            if (wparam == 'Z')
            {
                PostOwnerMessageFromChild(hwnd, kCaptureToolbarUndoAnnotationMessage);
                return 0;
            }
            if (wparam == 'Y')
            {
                PostOwnerMessageFromChild(hwnd, kCaptureToolbarRedoAnnotationMessage);
                return 0;
            }
        }
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

HWND CreateToolbarButton(
    HWND parent,
    int id,
    const wchar_t* text,
    int left,
    int width = kCommandButtonWidth,
    DWORD buttonStyle = 0)
{
    HWND button = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | buttonStyle,
        left,
        kButtonTop,
        width,
        kButtonHeight,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        nullptr,
        nullptr);
    ApplyDefaultFont(button);
    SetWindowSubclass(button, ToolbarButtonProc, 1, 0);
    return button;
}

void CreateToolbarSeparator(HWND parent, int left)
{
    CreateWindowExW(
        0,
        L"STATIC",
        nullptr,
        WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
        left,
        kSeparatorTop,
        kSeparatorWidth,
        kSeparatorHeight,
        parent,
        nullptr,
        nullptr,
        nullptr);
}

HWND CreateToolbarTooltip(HWND parent)
{
    HWND tooltip = CreateWindowExW(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASSW,
        nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (tooltip != nullptr)
    {
        SendMessageW(tooltip, TTM_ACTIVATE, TRUE, 0);
        SendMessageW(tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 400);
        SendMessageW(tooltip, TTM_SETDELAYTIME, TTDT_RESHOW, 100);
        SendMessageW(tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 5000);
        SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, 320);
        SetWindowPos(tooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    return tooltip;
}

void AddToolbarTooltip(HWND tooltip, HWND parent, HWND control, const wchar_t* text)
{
    if (tooltip == nullptr || control == nullptr)
    {
        return;
    }

    TOOLINFOW toolInfo{};
    toolInfo.cbSize = kTooltipInfoSize;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.hwnd = parent;
    toolInfo.uId = reinterpret_cast<UINT_PTR>(control);
    toolInfo.lpszText = const_cast<LPWSTR>(text);
    SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&toolInfo));
}

LRESULT CALLBACK ToolbarProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_NCCREATE:
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCTW>(lparam)->lpCreateParams));
        return TRUE;

    case WM_CREATE:
    {
        auto* toolbar = GetToolbar(hwnd);
        HWND tooltip = CreateToolbarTooltip(hwnd);
        int left = kButtonLeft;

        HWND closeButton = CreateToolbarButton(hwnd, kCancelButtonId, L"Close", left);
        AddToolbarTooltip(tooltip, hwnd, closeButton, L"Close capture (Esc)");
        left += kCommandButtonWidth + kButtonGap;
        CreateToolbarSeparator(hwnd, left + kSeparatorGap - kButtonGap);
        left += kSeparatorSpace - kButtonGap;

        HWND colorButton = CreateToolbarButton(hwnd, kColorButtonId, L"", left, kToolButtonWidth, BS_OWNERDRAW);
        AddToolbarTooltip(tooltip, hwnd, colorButton, L"Choose annotation color");
        left += kToolButtonWidth + kButtonGap;
        HWND arrowButton = CreateToolbarButton(hwnd, kArrowButtonId, L"", left, kToolButtonWidth, BS_OWNERDRAW);
        AddToolbarTooltip(tooltip, hwnd, arrowButton, L"Draw arrow");
        left += kToolButtonWidth + kButtonGap;
        HWND textButton = CreateToolbarButton(hwnd, kTextButtonId, L"", left, kToolButtonWidth, BS_OWNERDRAW);
        AddToolbarTooltip(tooltip, hwnd, textButton, L"Add text (Ctrl+T)");
        left += kToolButtonWidth + kButtonGap;
        CreateToolbarSeparator(hwnd, left + kSeparatorGap - kButtonGap);
        left += kSeparatorSpace - kButtonGap;

        if (IsOcrSupported())
        {
            HWND ocrButton = CreateToolbarButton(hwnd, kOcrButtonId, L"OCR", left);
            AddToolbarTooltip(tooltip, hwnd, ocrButton, L"Recognize text to clipboard (Ctrl+O)");
            left += kCommandButtonWidth + kButtonGap;
        }

        HWND saveButton = CreateToolbarButton(hwnd, kSaveButtonId, L"Save", left);
        AddToolbarTooltip(tooltip, hwnd, saveButton, L"Save selected area (Ctrl+S)");
        left += kCommandButtonWidth + kButtonGap;
        HWND copyButton = CreateToolbarButton(hwnd, kCopyButtonId, L"Copy", left);
        AddToolbarTooltip(tooltip, hwnd, copyButton, L"Copy selected area (Ctrl+C)");

        if (toolbar != nullptr)
        {
            toolbar->AttachTooltip(tooltip);
            toolbar->AttachArrowButton(arrowButton);
            toolbar->AttachColorButton(colorButton);
        }
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            PostOwnerMessage(hwnd, kCaptureToolbarCancelMessage);
            return 0;
        }
        if (IsCtrlPressed())
        {
            if (wparam == 'C')
            {
                PostOwnerMessage(hwnd, kCaptureToolbarCopyMessage);
                return 0;
            }
            if (wparam == 'S')
            {
                PostOwnerMessage(hwnd, kCaptureToolbarSaveMessage);
                return 0;
            }
            if (wparam == 'O')
            {
                if (IsOcrSupported())
                {
                    PostOwnerMessage(hwnd, kCaptureToolbarOcrMessage);
                }
                return 0;
            }
            if (wparam == 'T')
            {
                PostOwnerMessage(hwnd, kCaptureToolbarTextMessage);
                return 0;
            }
            if (wparam == 'Z')
            {
                PostOwnerMessage(hwnd, kCaptureToolbarUndoAnnotationMessage);
                return 0;
            }
            if (wparam == 'Y')
            {
                PostOwnerMessage(hwnd, kCaptureToolbarRedoAnnotationMessage);
                return 0;
            }
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

        case kOcrButtonId:
            if (HWND owner = GetWindow(hwnd, GW_OWNER))
            {
                if (IsOcrSupported())
                {
                    PostMessageW(owner, kCaptureToolbarOcrMessage, 0, 0);
                }
            }
            return 0;

        case kColorButtonId:
            if (HWND owner = GetWindow(hwnd, GW_OWNER))
            {
                PostMessageW(owner, kCaptureToolbarColorMessage, 0, 0);
            }
            return 0;

        case kArrowButtonId:
            if (HWND owner = GetWindow(hwnd, GW_OWNER))
            {
                PostMessageW(owner, kCaptureToolbarArrowMessage, 0, 0);
            }
            return 0;

        case kTextButtonId:
            if (HWND owner = GetWindow(hwnd, GW_OWNER))
            {
                PostMessageW(owner, kCaptureToolbarTextMessage, 0, 0);
            }
            return 0;

        default:
            break;
        }
        break;

    case WM_DRAWITEM:
    {
        const auto* drawItem = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        auto* toolbar = GetToolbar(hwnd);
        if (drawItem != nullptr && toolbar != nullptr &&
            (drawItem->CtlID == kColorButtonId || drawItem->CtlID == kArrowButtonId ||
                drawItem->CtlID == kTextButtonId))
        {
            UINT buttonState = DFCS_BUTTONPUSH;
            if ((drawItem->itemState & ODS_SELECTED) != 0)
            {
                buttonState |= DFCS_PUSHED;
            }
            if ((drawItem->itemState & ODS_DISABLED) != 0)
            {
                buttonState |= DFCS_INACTIVE;
            }

            DrawFrameControl(drawItem->hDC, const_cast<RECT*>(&drawItem->rcItem), DFC_BUTTON, buttonState);

            if (drawItem->CtlID == kArrowButtonId)
            {
                RECT content = drawItem->rcItem;
                InflateRect(&content, -9, -9);
                if ((drawItem->itemState & ODS_SELECTED) != 0)
                {
                    OffsetRect(&content, 1, 1);
                }

                const POINT head{content.left + 2, content.top + 2};
                const POINT tail{content.right - 2, content.bottom - 2};
                const int headSize = 6;

                HPEN pen = CreatePen(PS_SOLID, 3, toolbar->Color());
                HGDIOBJ oldPen = SelectObject(drawItem->hDC, pen);

                MoveToEx(drawItem->hDC, tail.x, tail.y, nullptr);
                LineTo(drawItem->hDC, head.x, head.y);
                LineTo(drawItem->hDC, head.x + headSize, head.y);
                MoveToEx(drawItem->hDC, head.x, head.y, nullptr);
                LineTo(drawItem->hDC, head.x, head.y + headSize);

                SelectObject(drawItem->hDC, oldPen);
                DeleteObject(pen);

                if ((drawItem->itemState & ODS_FOCUS) != 0)
                {
                    RECT focusRect = drawItem->rcItem;
                    InflateRect(&focusRect, -3, -3);
                    DrawFocusRect(drawItem->hDC, &focusRect);
                }

                return TRUE;
            }

            if (drawItem->CtlID == kTextButtonId)
            {
                RECT content = drawItem->rcItem;
                InflateRect(&content, -3, -2);
                if ((drawItem->itemState & ODS_SELECTED) != 0)
                {
                    OffsetRect(&content, 1, 1);
                }

                HFONT font = CreateFontW(
                    -24,
                    0,
                    0,
                    0,
                    FW_BOLD,
                    FALSE,
                    FALSE,
                    FALSE,
                    DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY,
                    VARIABLE_PITCH | FF_ROMAN,
                    L"Times New Roman");
                HGDIOBJ oldFont = font != nullptr ? SelectObject(drawItem->hDC, font) : nullptr;
                const int oldBkMode = SetBkMode(drawItem->hDC, TRANSPARENT);
                const COLORREF oldTextColor = SetTextColor(drawItem->hDC, toolbar->Color());

                DrawTextW(drawItem->hDC, L"T", 1, &content, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SetTextColor(drawItem->hDC, oldTextColor);
                SetBkMode(drawItem->hDC, oldBkMode);
                if (oldFont != nullptr)
                {
                    SelectObject(drawItem->hDC, oldFont);
                }
                if (font != nullptr)
                {
                    DeleteObject(font);
                }

                if ((drawItem->itemState & ODS_FOCUS) != 0)
                {
                    RECT focusRect = drawItem->rcItem;
                    InflateRect(&focusRect, -3, -3);
                    DrawFocusRect(drawItem->hDC, &focusRect);
                }

                return TRUE;
            }

            RECT swatch = drawItem->rcItem;
            InflateRect(&swatch, -4, -4);
            if ((drawItem->itemState & ODS_SELECTED) != 0)
            {
                OffsetRect(&swatch, 1, 1);
            }

            HBRUSH brush = CreateSolidBrush(toolbar->Color());
            FillRect(drawItem->hDC, &swatch, brush);
            DeleteObject(brush);

            FrameRect(drawItem->hDC, &swatch, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

            if ((drawItem->itemState & ODS_FOCUS) != 0)
            {
                RECT focusRect = drawItem->rcItem;
                InflateRect(&focusRect, -3, -3);
                DrawFocusRect(drawItem->hDC, &focusRect);
            }

            return TRUE;
        }
        break;
    }

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

void InitializeCaptureToolbarOcrSupport()
{
    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&commonControls);

    gIsOcrSupported = DetectOcrSupport();
}

CaptureToolbar::~CaptureToolbar()
{
    Hide();
}

bool CaptureToolbar::Show(HWND owner, POINT anchorScreenPosition, COLORREF color)
{
    SetColor(color);

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
            GetToolbarWidth(),
            kToolbarHeight,
            owner,
            nullptr,
            instance,
            this);

        if (hwnd_ == nullptr)
        {
            return false;
        }
    }

    SetColor(color);
    PlaceBelowSelection(hwnd_, anchorScreenPosition);
    return true;
}

void CaptureToolbar::Hide()
{
    if (tooltip_ != nullptr && IsWindow(tooltip_))
    {
        DestroyWindow(tooltip_);
    }

    if (hwnd_ != nullptr && IsWindow(hwnd_))
    {
        DestroyWindow(hwnd_);
    }

    hwnd_ = nullptr;
    tooltip_ = nullptr;
    colorButton_ = nullptr;
    arrowButton_ = nullptr;
}

void CaptureToolbar::SetColor(COLORREF color)
{
    color_ = color;
    if (colorButton_ != nullptr && IsWindow(colorButton_))
    {
        InvalidateRect(colorButton_, nullptr, TRUE);
    }
    if (arrowButton_ != nullptr && IsWindow(arrowButton_))
    {
        InvalidateRect(arrowButton_, nullptr, TRUE);
    }
}

void CaptureToolbar::AttachTooltip(HWND tooltip)
{
    tooltip_ = tooltip;
}

void CaptureToolbar::AttachColorButton(HWND button)
{
    colorButton_ = button;
    SetColor(color_);
}

void CaptureToolbar::AttachArrowButton(HWND button)
{
    arrowButton_ = button;
    SetColor(color_);
}

HWND CaptureToolbar::Tooltip() const
{
    return tooltip_;
}

COLORREF CaptureToolbar::Color() const
{
    return color_;
}
