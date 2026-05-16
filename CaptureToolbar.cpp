#include "CaptureToolbar.h"

#include <algorithm>
#include <commctrl.h>

namespace
{
constexpr wchar_t kToolbarClassName[] = L"PrtScCaptureToolbar";
constexpr int kToolbarWidth = 573;
constexpr int kToolbarHeight = 58;
constexpr int kToolbarMargin = 8;
constexpr int kButtonWidth = 104;
constexpr int kButtonHeight = 36;
constexpr int kButtonTop = 10;
constexpr int kButtonLeft = 12;
constexpr int kButtonGap = 8;
constexpr int kCopyButtonId = 3001;
constexpr int kSaveButtonId = 3002;
constexpr int kCancelButtonId = 3003;
constexpr int kColorButtonId = 3004;
constexpr int kArrowButtonId = 3005;

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
        }
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

HWND CreateToolbarButton(
    HWND parent,
    int id,
    const wchar_t* text,
    int left,
    int width = kButtonWidth,
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
        CreateToolbarButton(hwnd, kCopyButtonId, L"Copy", kButtonLeft);
        CreateToolbarButton(hwnd, kSaveButtonId, L"Save", kButtonLeft + (kButtonWidth + kButtonGap));
        HWND arrowButton = CreateToolbarButton(hwnd, kArrowButtonId, L"", kButtonLeft + ((kButtonWidth + kButtonGap) * 2), kButtonWidth, BS_OWNERDRAW);
        HWND colorButton = CreateToolbarButton(hwnd, kColorButtonId, L"", kButtonLeft + ((kButtonWidth + kButtonGap) * 3), kButtonWidth, BS_OWNERDRAW);
        CreateToolbarButton(hwnd, kCancelButtonId, L"Cancel", kButtonLeft + ((kButtonWidth + kButtonGap) * 4));

        if (toolbar != nullptr)
        {
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

        default:
            break;
        }
        break;

    case WM_DRAWITEM:
    {
        const auto* drawItem = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        auto* toolbar = GetToolbar(hwnd);
        if (drawItem != nullptr && toolbar != nullptr && (drawItem->CtlID == kColorButtonId || drawItem->CtlID == kArrowButtonId))
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
                InflateRect(&content, -10, -8);
                if ((drawItem->itemState & ODS_SELECTED) != 0)
                {
                    OffsetRect(&content, 1, 1);
                }

                const int y = content.top + ((content.bottom - content.top) / 2);
                const int arrowLength = MulDiv(content.right - content.left, 80, 100);
                const int startX = content.left + ((content.right - content.left - arrowLength) / 2);
                const int endX = startX + arrowLength;
                const int headSize = 8;

                HPEN pen = CreatePen(PS_SOLID, 3, toolbar->Color());
                HGDIOBJ oldPen = SelectObject(drawItem->hDC, pen);

                MoveToEx(drawItem->hDC, endX, y, nullptr);
                LineTo(drawItem->hDC, startX, y);
                LineTo(drawItem->hDC, startX + headSize, y - headSize);
                MoveToEx(drawItem->hDC, startX, y, nullptr);
                LineTo(drawItem->hDC, startX + headSize, y + headSize);

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

            RECT swatch = drawItem->rcItem;
            InflateRect(&swatch, -8, -7);
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
            kToolbarWidth,
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
    if (hwnd_ != nullptr && IsWindow(hwnd_))
    {
        DestroyWindow(hwnd_);
    }

    hwnd_ = nullptr;
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

COLORREF CaptureToolbar::Color() const
{
    return color_;
}
