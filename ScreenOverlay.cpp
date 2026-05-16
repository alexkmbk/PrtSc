#include "ScreenOverlay.h"

#include "CaptureToolbar.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <commdlg.h>
#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>

namespace
{
constexpr wchar_t kOverlayClassName[] = L"PrtScScreenOverlay";
constexpr BYTE kOverlayAlpha = 110;
// Alpha 0 pixels in a layered window are click-through. Alpha 1 is visually
// transparent, but keeps the selected area interactive for moving/resizing.
constexpr uint32_t kTransparentPixel = 0x01000000;
constexpr uint32_t kDimPixel = (static_cast<uint32_t>(kOverlayAlpha) << 24);
constexpr uint32_t kBorderPixel = 0xFFFFFFFF;

enum class SelectionMode
{
    None,
    Create,
    Move,
};

struct OverlayState
{
    SIZE size{};
    POINT dragStart{};
    POINT moveOffset{};
    RECT selection{};
    RECT previousSelection{};
    CaptureToolbar toolbar{};
    SelectionMode mode = SelectionMode::None;
    bool hasSelection = false;
    bool isMouseDown = false;
    bool isDragging = false;
};

POINT GetClientPoint(LPARAM lparam)
{
    return {
        GET_X_LPARAM(lparam),
        GET_Y_LPARAM(lparam),
    };
}

RECT NormalizeRect(POINT start, POINT end)
{
    return {
        std::min(start.x, end.x),
        std::min(start.y, end.y),
        std::max(start.x, end.x),
        std::max(start.y, end.y),
    };
}

bool IsRectVisible(const RECT& rect)
{
    return rect.right > rect.left && rect.bottom > rect.top;
}

bool IsPointInsideRect(POINT point, const RECT& rect)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

LONG ClampLong(LONG value, LONG low, LONG high)
{
    return std::clamp(value, low, high);
}

RECT ClampRectToOverlay(const RECT& rect, SIZE size)
{
    return {
        ClampLong(rect.left, 0, static_cast<LONG>(size.cx)),
        ClampLong(rect.top, 0, static_cast<LONG>(size.cy)),
        ClampLong(rect.right, 0, static_cast<LONG>(size.cx)),
        ClampLong(rect.bottom, 0, static_cast<LONG>(size.cy)),
    };
}

RECT MoveRectToPoint(const RECT& rect, POINT point, POINT offset, SIZE size)
{
    const LONG width = rect.right - rect.left;
    const LONG height = rect.bottom - rect.top;
    const LONG left = ClampLong(point.x - offset.x, 0, static_cast<LONG>(size.cx) - width);
    const LONG top = ClampLong(point.y - offset.y, 0, static_cast<LONG>(size.cy) - height);

    return {
        left,
        top,
        left + width,
        top + height,
    };
}

void DrawHorizontalLine(std::vector<uint32_t>& pixels, SIZE size, int y, int left, int right, uint32_t color)
{
    if (y < 0 || y >= size.cy)
    {
        return;
    }

    left = std::clamp(left, 0, static_cast<int>(size.cx));
    right = std::clamp(right, 0, static_cast<int>(size.cx));

    for (int x = left; x < right; ++x)
    {
        pixels[static_cast<size_t>(y) * size.cx + x] = color;
    }
}

void DrawVerticalLine(std::vector<uint32_t>& pixels, SIZE size, int x, int top, int bottom, uint32_t color)
{
    if (x < 0 || x >= size.cx)
    {
        return;
    }

    top = std::clamp(top, 0, static_cast<int>(size.cy));
    bottom = std::clamp(bottom, 0, static_cast<int>(size.cy));

    for (int y = top; y < bottom; ++y)
    {
        pixels[static_cast<size_t>(y) * size.cx + x] = color;
    }
}

void DrawTransparentSelection(std::vector<uint32_t>& pixels, SIZE size, RECT selection)
{
    selection = ClampRectToOverlay(selection, size);
    if (!IsRectVisible(selection))
    {
        return;
    }

    for (int y = selection.top; y < selection.bottom; ++y)
    {
        const size_t rowStart = static_cast<size_t>(y) * size.cx;
        for (int x = selection.left; x < selection.right; ++x)
        {
            pixels[rowStart + x] = kTransparentPixel;
        }
    }

    DrawHorizontalLine(pixels, size, selection.top, selection.left, selection.right, kBorderPixel);
    DrawHorizontalLine(pixels, size, selection.bottom - 1, selection.left, selection.right, kBorderPixel);
    DrawVerticalLine(pixels, size, selection.left, selection.top, selection.bottom, kBorderPixel);
    DrawVerticalLine(pixels, size, selection.right - 1, selection.top, selection.bottom, kBorderPixel);
}

void RenderOverlay(HWND hwnd, const OverlayState& state)
{
    std::vector<uint32_t> pixels(static_cast<size_t>(state.size.cx) * state.size.cy, kDimPixel);
    if (state.hasSelection)
    {
        DrawTransparentSelection(pixels, state.size, state.selection);
    }

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = state.size.cx;
    bitmapInfo.bmiHeader.biHeight = -state.size.cy;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bitmapBits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits, nullptr, 0);
    if (bitmap != nullptr && bitmapBits != nullptr)
    {
        std::copy(pixels.begin(), pixels.end(), static_cast<uint32_t*>(bitmapBits));

        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
        POINT sourcePoint{};
        POINT windowPoint{};
        RECT windowRect{};
        GetWindowRect(hwnd, &windowRect);
        windowPoint.x = windowRect.left;
        windowPoint.y = windowRect.top;

        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        SIZE layerSize = state.size;
        UpdateLayeredWindow(hwnd, screenDc, &windowPoint, &layerSize, memoryDc, &sourcePoint, 0, &blend, ULW_ALPHA);
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
    }

    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
}

void ShowToolbarForSelection(HWND hwnd, OverlayState& state)
{
    POINT anchorPoint{state.selection.right, state.selection.bottom};
    ClientToScreen(hwnd, &anchorPoint);
    state.toolbar.Show(hwnd, anchorPoint);
}

RECT SelectionToScreenRect(HWND hwnd, const RECT& selection)
{
    POINT topLeft{selection.left, selection.top};
    POINT bottomRight{selection.right, selection.bottom};
    ClientToScreen(hwnd, &topLeft);
    ClientToScreen(hwnd, &bottomRight);

    return {
        topLeft.x,
        topLeft.y,
        bottomRight.x,
        bottomRight.y,
    };
}

HBITMAP CaptureScreenRect(const RECT& screenRect)
{
    const int width = screenRect.right - screenRect.left;
    const int height = screenRect.bottom - screenRect.top;
    if (width <= 0 || height <= 0)
    {
        return nullptr;
    }

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    if (bitmap == nullptr)
    {
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return nullptr;
    }

    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
    const BOOL copied = BitBlt(memoryDc, 0, 0, width, height, screenDc, screenRect.left, screenRect.top, SRCCOPY);
    SelectObject(memoryDc, oldBitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (!copied)
    {
        DeleteObject(bitmap);
        return nullptr;
    }

    return bitmap;
}

bool CopyBitmapToClipboard(HWND owner, HBITMAP bitmap)
{
    if (!OpenClipboard(owner))
    {
        return false;
    }

    EmptyClipboard();
    if (SetClipboardData(CF_BITMAP, bitmap) == nullptr)
    {
        CloseClipboard();
        DeleteObject(bitmap);
        return false;
    }

    CloseClipboard();
    return true;
}

bool CopyScreenRectToClipboard(HWND owner, const RECT& screenRect)
{
    HBITMAP bitmap = CaptureScreenRect(screenRect);
    if (bitmap == nullptr)
    {
        return false;
    }

    if (!CopyBitmapToClipboard(owner, bitmap))
    {
        DeleteObject(bitmap);
        return false;
    }

    return true;
}

bool GetPngEncoderClsid(CLSID& clsid)
{
    UINT encoderCount = 0;
    UINT encoderBytes = 0;
    if (Gdiplus::GetImageEncodersSize(&encoderCount, &encoderBytes) != Gdiplus::Ok || encoderBytes == 0)
    {
        return false;
    }

    std::vector<BYTE> buffer(encoderBytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(encoderCount, encoderBytes, encoders) != Gdiplus::Ok)
    {
        return false;
    }

    for (UINT i = 0; i < encoderCount; ++i)
    {
        if (wcscmp(encoders[i].MimeType, L"image/png") == 0)
        {
            clsid = encoders[i].Clsid;
            return true;
        }
    }

    return false;
}

std::filesystem::path MakeUniquePngPath(std::filesystem::path path)
{
    if (path.extension().empty())
    {
        path.replace_extension(L".png");
    }

    if (!std::filesystem::exists(path))
    {
        return path;
    }

    const std::filesystem::path directory = path.parent_path();
    const std::wstring stem = path.stem().wstring();
    const std::wstring extension = path.extension().wstring();

    for (int index = 1;; ++index)
    {
        std::filesystem::path candidate = directory / (stem + L"_" + std::to_wstring(index) + extension);
        if (!std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }
}

std::filesystem::path GetDefaultScreenshotPath()
{
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);

    wchar_t filename[64]{};
    swprintf_s(
        filename,
        L"prtsc_%04u-%02u-%02u-%02u%02u.png",
        localTime.wYear,
        localTime.wMonth,
        localTime.wDay,
        localTime.wHour,
        localTime.wMinute);

    return MakeUniquePngPath(std::filesystem::current_path() / filename);
}

bool ShowSavePngDialog(std::filesystem::path& selectedPath)
{
    std::filesystem::path defaultPath = GetDefaultScreenshotPath();
    wchar_t filePath[MAX_PATH]{};
    wcscpy_s(filePath, defaultPath.wstring().c_str());

    OPENFILENAMEW openFileName{};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.hwndOwner = nullptr;
    openFileName.lpstrFilter = L"PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0";
    openFileName.lpstrFile = filePath;
    openFileName.nMaxFile = static_cast<DWORD>(sizeof(filePath) / sizeof(filePath[0]));
    openFileName.lpstrDefExt = L"png";
    openFileName.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

    if (GetSaveFileNameW(&openFileName) == FALSE)
    {
        return false;
    }

    selectedPath = MakeUniquePngPath(filePath);
    return true;
}

bool SaveBitmapAsPng(HBITMAP bitmap, const std::filesystem::path& path)
{
    Gdiplus::GdiplusStartupInput startupInput{};
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
    {
        return false;
    }

    CLSID pngClsid{};
    const bool hasPngEncoder = GetPngEncoderClsid(pngClsid);
    bool saved = false;
    if (hasPngEncoder)
    {
        Gdiplus::Bitmap gdiplusBitmap(bitmap, nullptr);
        saved = gdiplusBitmap.Save(path.wstring().c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return saved;
}

void CopySelectionToClipboardAndClose(HWND hwnd, OverlayState& state)
{
    if (!state.hasSelection || !IsRectVisible(state.selection))
    {
        return;
    }

    const RECT screenRect = SelectionToScreenRect(hwnd, state.selection);
    state.toolbar.Hide();
    ShowWindow(hwnd, SW_HIDE);
    Sleep(80);

    CopyScreenRectToClipboard(hwnd, screenRect);
    DestroyWindow(hwnd);
}

void SaveSelectionToFileAndClose(HWND hwnd, OverlayState& state)
{
    if (!state.hasSelection || !IsRectVisible(state.selection))
    {
        return;
    }

    const RECT screenRect = SelectionToScreenRect(hwnd, state.selection);
    state.toolbar.Hide();
    ShowWindow(hwnd, SW_HIDE);
    Sleep(80);

    HBITMAP bitmap = CaptureScreenRect(screenRect);
    if (bitmap != nullptr)
    {
        std::filesystem::path selectedPath;
        if (ShowSavePngDialog(selectedPath))
        {
            SaveBitmapAsPng(bitmap, selectedPath);
        }

        DeleteObject(bitmap);
    }

    DestroyWindow(hwnd);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* state = reinterpret_cast<OverlayState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case kCaptureToolbarCopyMessage:
        if (state != nullptr)
        {
            CopySelectionToClipboardAndClose(hwnd, *state);
        }
        return 0;

    case kCaptureToolbarSaveMessage:
        if (state != nullptr)
        {
            SaveSelectionToFileAndClose(hwnd, *state);
        }
        return 0;

    case kCaptureToolbarCancelMessage:
        DestroyWindow(hwnd);
        return 0;

    case WM_CREATE:
    {
        auto* createdState = reinterpret_cast<OverlayState*>(reinterpret_cast<LPCREATESTRUCTW>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createdState));
        RenderOverlay(hwnd, *createdState);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (state != nullptr)
        {
            const POINT point = GetClientPoint(lparam);

            SetCapture(hwnd);
            SetFocus(hwnd);
            state->isMouseDown = true;
            state->isDragging = false;
            state->dragStart = point;
            state->previousSelection = state->selection;
            state->toolbar.Hide();

            if (state->hasSelection && IsPointInsideRect(point, state->selection))
            {
                state->mode = SelectionMode::Move;
                state->moveOffset = {
                    point.x - state->selection.left,
                    point.y - state->selection.top,
                };
            }
            else
            {
                state->mode = SelectionMode::Create;
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (state != nullptr && state->isMouseDown && (wparam & MK_LBUTTON) != 0)
        {
            const POINT current = GetClientPoint(lparam);
            if (state->mode == SelectionMode::Move && state->hasSelection)
            {
                state->isDragging = true;
                state->selection = MoveRectToPoint(state->previousSelection, current, state->moveOffset, state->size);
                RenderOverlay(hwnd, *state);
            }
            else if (state->mode == SelectionMode::Create)
            {
                RECT selection = NormalizeRect(state->dragStart, current);

                if (IsRectVisible(selection))
                {
                    state->isDragging = true;
                    state->hasSelection = true;
                    state->selection = selection;
                    RenderOverlay(hwnd, *state);
                }
            }
        }
        else if (state != nullptr)
        {
            const POINT current = GetClientPoint(lparam);
            const bool isInsideSelection = state->hasSelection && IsPointInsideRect(current, state->selection);
            SetCursor(LoadCursorW(nullptr, isInsideSelection ? IDC_SIZEALL : IDC_CROSS));
        }
        return 0;

    case WM_LBUTTONUP:
        if (state != nullptr)
        {
            const bool wasDragging = state->isDragging;

            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }

            if (!wasDragging)
            {
                state->selection = state->previousSelection;
                state->hasSelection = IsRectVisible(state->selection);
                RenderOverlay(hwnd, *state);
            }
            else
            {
                state->hasSelection = IsRectVisible(state->selection);
                RenderOverlay(hwnd, *state);
                ShowToolbarForSelection(hwnd, *state);
            }

            state->isMouseDown = false;
            state->isDragging = false;
            state->mode = SelectionMode::None;
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (state != nullptr)
        {
            state->isMouseDown = false;
            state->isDragging = false;
            state->mode = SelectionMode::None;
        }
        return 0;

    case WM_SETCURSOR:
        if (state != nullptr)
        {
            POINT cursor{};
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);

            const bool isInsideSelection = state->hasSelection && IsPointInsideRect(cursor, state->selection);
            SetCursor(LoadCursorW(nullptr, isInsideSelection ? IDC_SIZEALL : IDC_CROSS));
        }
        else
        {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        }
        return 0;

    case WM_DESTROY:
    {
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool RegisterOverlayClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = OverlayProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = kOverlayClassName;

    if (RegisterClassExW(&windowClass) != 0)
    {
        return true;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
}

bool ScreenOverlay::Show(HINSTANCE instance)
{
    if (hwnd_ != nullptr && IsWindow(hwnd_))
    {
        SetForegroundWindow(hwnd_);
        return true;
    }

    if (!RegisterOverlayClass(instance))
    {
        return false;
    }

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    auto state = std::make_unique<OverlayState>();
    state->size = {width, height};

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kOverlayClassName,
        L"PrtSc Overlay",
        WS_POPUP,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        state.get());

    if (hwnd_ == nullptr)
    {
        return false;
    }

    state.release();
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    return true;
}

void ScreenOverlay::Close()
{
    if (hwnd_ != nullptr && IsWindow(hwnd_))
    {
        DestroyWindow(hwnd_);
    }

    hwnd_ = nullptr;
}
