#include "ScreenOverlay.h"

#include "CaptureToolbar.h"
#include "OcrEngine.h"
#include "Settings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <commdlg.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <windowsx.h>

namespace
{
constexpr wchar_t kOverlayClassName[] = L"PrtScScreenOverlay";
constexpr BYTE kOverlayAlpha = 110;
constexpr uint32_t kDimPixel = (static_cast<uint32_t>(kOverlayAlpha) << 24);
constexpr uint32_t kBorderPixel = 0xFFFFFFFF;
constexpr float kArrowWidth = 4.0F;
constexpr float kArrowHeadLength = 16.0F;
constexpr float kArrowHeadAngleRadians = 0.65F;
constexpr ULONGLONG kRenderThrottleMilliseconds = 16;

enum class SelectionMode
{
    None,
    Create,
    Move,
    Arrow,
};

struct ArrowLine
{
    POINT start{};
    POINT end{};
    COLORREF color = RGB(255, 0, 0);
    bool isVisible = true;
};

struct OverlayState
{
    ~OverlayState()
    {
        if (renderBitmap != nullptr)
        {
            DeleteObject(renderBitmap);
        }

        if (hasGdiplus)
        {
            Gdiplus::GdiplusShutdown(gdiplusToken);
        }
    }

    SIZE size{};
    POINT screenOrigin{};
    std::vector<uint32_t> screenshotPixels;
    std::vector<uint32_t> dimmedScreenshotPixels;
    std::vector<uint32_t> renderPixels;
    HBITMAP renderBitmap = nullptr;
    void* renderBitmapBits = nullptr;
    ULONG_PTR gdiplusToken = 0;
    ULONGLONG lastRenderTick = 0;
    POINT dragStart{};
    POINT moveOffset{};
    RECT selection{};
    RECT previousSelection{};
    CaptureToolbar toolbar{};
    std::vector<ArrowLine> arrows;
    std::vector<ArrowLine> previousArrows;
    ArrowLine previewArrow{};
    COLORREF annotationColor = RGB(255, 0, 0);
    int currentArrowIndex = -1;
    SelectionMode mode = SelectionMode::None;
    bool isArrowToolActive = false;
    bool hasPreviewArrow = false;
    bool hasSelection = false;
    bool isMouseDown = false;
    bool isDragging = false;
    bool hasGdiplus = false;
};

struct SelectionPixels
{
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;
};

bool StartGdiplus(ULONG_PTR& gdiplusToken)
{
    Gdiplus::GdiplusStartupInput startupInput{};
    return Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) == Gdiplus::Ok;
}

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

bool IsCtrlPressed()
{
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

int GetLastVisibleArrowIndex(const std::vector<ArrowLine>& arrows)
{
    for (int index = static_cast<int>(arrows.size()) - 1; index >= 0; --index)
    {
        if (arrows[static_cast<size_t>(index)].isVisible)
        {
            return index;
        }
    }

    return -1;
}

void ResetArrowUndoState(OverlayState& state)
{
    state.currentArrowIndex = GetLastVisibleArrowIndex(state.arrows);
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

std::vector<ArrowLine> MoveArrows(const std::vector<ArrowLine>& arrows, LONG dx, LONG dy)
{
    std::vector<ArrowLine> movedArrows = arrows;
    for (ArrowLine& arrow : movedArrows)
    {
        arrow.start.x += dx;
        arrow.start.y += dy;
        arrow.end.x += dx;
        arrow.end.y += dy;
    }

    return movedArrows;
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

uint32_t WithOpaqueAlpha(uint32_t color)
{
    return 0xFF000000 | (color & 0x00FFFFFF);
}

uint32_t WithoutAlpha(uint32_t color)
{
    return color & 0x00FFFFFF;
}

uint32_t DimScreenshotPixel(uint32_t color)
{
    constexpr uint32_t kDimNumerator = 255 - kOverlayAlpha;
    const uint32_t red = ((color >> 16) & 0xFF) * kDimNumerator / 255;
    const uint32_t green = ((color >> 8) & 0xFF) * kDimNumerator / 255;
    const uint32_t blue = (color & 0xFF) * kDimNumerator / 255;
    return 0xFF000000 | (red << 16) | (green << 8) | blue;
}

void DrawSelectionFromScreenshot(std::vector<uint32_t>& pixels, const OverlayState& state, RECT selection)
{
    selection = ClampRectToOverlay(selection, state.size);
    if (!IsRectVisible(selection) || state.screenshotPixels.empty())
    {
        return;
    }

    for (int y = selection.top; y < selection.bottom; ++y)
    {
        const size_t rowStart = static_cast<size_t>(y) * state.size.cx;
        for (int x = selection.left; x < selection.right; ++x)
        {
            const size_t index = rowStart + x;
            pixels[index] = WithOpaqueAlpha(state.screenshotPixels[index]);
        }
    }

    DrawHorizontalLine(pixels, state.size, selection.top, selection.left, selection.right, kBorderPixel);
    DrawHorizontalLine(pixels, state.size, selection.bottom - 1, selection.left, selection.right, kBorderPixel);
    DrawVerticalLine(pixels, state.size, selection.left, selection.top, selection.bottom, kBorderPixel);
    DrawVerticalLine(pixels, state.size, selection.right - 1, selection.top, selection.bottom, kBorderPixel);
}

Gdiplus::Color ToGdiplusColor(COLORREF color)
{
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

void DrawArrowLine(Gdiplus::Graphics& graphics, const ArrowLine& arrow, int offsetX = 0, int offsetY = 0)
{
    const float startX = static_cast<float>(arrow.start.x - offsetX);
    const float startY = static_cast<float>(arrow.start.y - offsetY);
    const float endX = static_cast<float>(arrow.end.x - offsetX);
    const float endY = static_cast<float>(arrow.end.y - offsetY);
    const float dx = endX - startX;
    const float dy = endY - startY;
    const float length = std::sqrt((dx * dx) + (dy * dy));
    if (length < 2.0F)
    {
        return;
    }

    Gdiplus::Pen pen(ToGdiplusColor(arrow.color), kArrowWidth);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    graphics.DrawLine(&pen, startX, startY, endX, endY);

    const float angle = std::atan2(dy, dx);
    const float leftAngle = angle + 3.14159265F - kArrowHeadAngleRadians;
    const float rightAngle = angle + 3.14159265F + kArrowHeadAngleRadians;

    graphics.DrawLine(
        &pen,
        endX,
        endY,
        endX + std::cos(leftAngle) * kArrowHeadLength,
        endY + std::sin(leftAngle) * kArrowHeadLength);
    graphics.DrawLine(
        &pen,
        endX,
        endY,
        endX + std::cos(rightAngle) * kArrowHeadLength,
        endY + std::sin(rightAngle) * kArrowHeadLength);
}

void DrawArrows(Gdiplus::Graphics& graphics, const std::vector<ArrowLine>& arrows, int offsetX = 0, int offsetY = 0)
{
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    for (const ArrowLine& arrow : arrows)
    {
        if (arrow.isVisible)
        {
            DrawArrowLine(graphics, arrow, offsetX, offsetY);
        }
    }
}

bool EnsureRenderBitmap(OverlayState& state, HDC screenDc)
{
    if (state.renderBitmap != nullptr && state.renderBitmapBits != nullptr)
    {
        return true;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = state.size.cx;
    bitmapInfo.bmiHeader.biHeight = -state.size.cy;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    state.renderBitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &state.renderBitmapBits, nullptr, 0);
    if (state.renderBitmap == nullptr || state.renderBitmapBits == nullptr)
    {
        if (state.renderBitmap != nullptr)
        {
            DeleteObject(state.renderBitmap);
            state.renderBitmap = nullptr;
        }

        state.renderBitmapBits = nullptr;
        return false;
    }

    return true;
}

void BuildOverlayPixels(OverlayState& state)
{
    const size_t pixelCount = static_cast<size_t>(state.size.cx) * state.size.cy;
    state.renderPixels.resize(pixelCount);
    if (state.dimmedScreenshotPixels.size() == pixelCount)
    {
        std::copy(state.dimmedScreenshotPixels.begin(), state.dimmedScreenshotPixels.end(), state.renderPixels.begin());
    }
    else
    {
        std::fill(state.renderPixels.begin(), state.renderPixels.end(), kDimPixel);
    }

    if (state.hasSelection)
    {
        DrawSelectionFromScreenshot(state.renderPixels, state, state.selection);
    }
}

void RenderOverlay(HWND hwnd, OverlayState& state)
{
    state.lastRenderTick = GetTickCount64();
    BuildOverlayPixels(state);
    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr)
    {
        return;
    }

    HDC memoryDc = CreateCompatibleDC(screenDc);
    if (memoryDc != nullptr && EnsureRenderBitmap(state, screenDc))
    {
        std::copy(state.renderPixels.begin(), state.renderPixels.end(), static_cast<uint32_t*>(state.renderBitmapBits));

        HGDIOBJ oldBitmap = SelectObject(memoryDc, state.renderBitmap);
        if (state.hasGdiplus)
        {
            Gdiplus::Graphics graphics(memoryDc);
            DrawArrows(graphics, state.arrows);
            if (state.hasPreviewArrow)
            {
                DrawArrowLine(graphics, state.previewArrow);
            }
        }

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
    }

    if (memoryDc != nullptr)
    {
        DeleteDC(memoryDc);
    }

    ReleaseDC(nullptr, screenDc);
}

void RenderOverlayThrottled(HWND hwnd, OverlayState& state)
{
    const ULONGLONG now = GetTickCount64();
    if (state.lastRenderTick != 0 && now - state.lastRenderTick < kRenderThrottleMilliseconds)
    {
        return;
    }

    RenderOverlay(hwnd, state);
}

void ShowToolbarForSelection(HWND hwnd, OverlayState& state)
{
    POINT anchorPoint{state.selection.right, state.selection.bottom};
    ClientToScreen(hwnd, &anchorPoint);
    state.toolbar.Show(hwnd, anchorPoint, state.annotationColor);
}

POINT ClientPointToScreen(HWND hwnd, POINT point)
{
    ClientToScreen(hwnd, &point);
    return point;
}

POINT ClampPointToRect(POINT point, const RECT& rect)
{
    return {
        ClampLong(point.x, rect.left, rect.right),
        ClampLong(point.y, rect.top, rect.bottom),
    };
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

bool CaptureScreenSnapshot(OverlayState& state, const RECT& screenRect)
{
    const int width = screenRect.right - screenRect.left;
    const int height = screenRect.bottom - screenRect.top;
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bitmapBits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits, nullptr, 0);
    if (bitmap == nullptr || bitmapBits == nullptr)
    {
        if (bitmap != nullptr)
        {
            DeleteObject(bitmap);
        }
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
    const BOOL copied = BitBlt(memoryDc, 0, 0, width, height, screenDc, screenRect.left, screenRect.top, SRCCOPY);
    SelectObject(memoryDc, oldBitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (!copied)
    {
        DeleteObject(bitmap);
        return false;
    }

    state.size = {width, height};
    state.screenOrigin = {screenRect.left, screenRect.top};
    const size_t pixelCount = static_cast<size_t>(width) * height;
    state.screenshotPixels.assign(static_cast<uint32_t*>(bitmapBits), static_cast<uint32_t*>(bitmapBits) + pixelCount);
    state.dimmedScreenshotPixels.resize(pixelCount);
    std::transform(
        state.screenshotPixels.begin(),
        state.screenshotPixels.end(),
        state.dimmedScreenshotPixels.begin(),
        DimScreenshotPixel);
    state.renderPixels.resize(pixelCount);
    DeleteObject(bitmap);
    return true;
}

void DrawArrowsOnSelectionPixels(const OverlayState& state, SelectionPixels& selectionPixels, int offsetX, int offsetY)
{
    if (state.arrows.empty() || selectionPixels.width <= 0 || selectionPixels.height <= 0 || selectionPixels.pixels.empty())
    {
        return;
    }

    ULONG_PTR gdiplusToken = 0;
    if (!StartGdiplus(gdiplusToken))
    {
        return;
    }

    {
        Gdiplus::Bitmap bitmap(
            selectionPixels.width,
            selectionPixels.height,
            selectionPixels.width * static_cast<int>(sizeof(uint32_t)),
            PixelFormat32bppRGB,
            reinterpret_cast<BYTE*>(selectionPixels.pixels.data()));
        Gdiplus::Graphics graphics(&bitmap);
        DrawArrows(graphics, state.arrows, offsetX, offsetY);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
}

bool GetSelectionPixels(const OverlayState& state, const RECT& screenRect, SelectionPixels& selectionPixels)
{
    selectionPixels = {};

    const int width = screenRect.right - screenRect.left;
    const int height = screenRect.bottom - screenRect.top;
    if (width <= 0 || height <= 0 || state.screenshotPixels.empty())
    {
        return false;
    }

    const int sourceLeft = screenRect.left - state.screenOrigin.x;
    const int sourceTop = screenRect.top - state.screenOrigin.y;
    if (sourceLeft < 0 || sourceTop < 0 || sourceLeft + width > state.size.cx || sourceTop + height > state.size.cy)
    {
        return false;
    }

    selectionPixels.width = width;
    selectionPixels.height = height;
    selectionPixels.pixels.resize(static_cast<size_t>(width) * height);

    auto* destination = selectionPixels.pixels.data();
    for (int y = 0; y < height; ++y)
    {
        const size_t sourceIndex = static_cast<size_t>(sourceTop + y) * state.size.cx + sourceLeft;
        const size_t destinationIndex = static_cast<size_t>(y) * width;
        std::transform(
            state.screenshotPixels.data() + sourceIndex,
            state.screenshotPixels.data() + sourceIndex + width,
            destination + destinationIndex,
            WithoutAlpha);
    }

    DrawArrowsOnSelectionPixels(state, selectionPixels, sourceLeft, sourceTop);
    return true;
}

HBITMAP CreateBitmapFromSelectionPixels(const SelectionPixels& selectionPixels)
{
    if (selectionPixels.width <= 0 || selectionPixels.height <= 0 || selectionPixels.pixels.empty())
    {
        return nullptr;
    }

    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr)
    {
        return nullptr;
    }

    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, selectionPixels.width, selectionPixels.height);
    if (bitmap == nullptr)
    {
        ReleaseDC(nullptr, screenDc);
        return nullptr;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = selectionPixels.width;
    bitmapInfo.bmiHeader.biHeight = -selectionPixels.height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    const int scanLines = SetDIBits(
        screenDc,
        bitmap,
        0,
        static_cast<UINT>(selectionPixels.height),
        selectionPixels.pixels.data(),
        &bitmapInfo,
        DIB_RGB_COLORS);
    ReleaseDC(nullptr, screenDc);
    if (scanLines != selectionPixels.height)
    {
        DeleteObject(bitmap);
        return nullptr;
    }

    return bitmap;
}

bool CopyDibToClipboard(HWND owner, const SelectionPixels& selectionPixels)
{
    if (selectionPixels.width <= 0 || selectionPixels.height <= 0 || selectionPixels.pixels.empty())
    {
        return false;
    }

    const size_t headerSize = sizeof(BITMAPINFOHEADER);
    const size_t pixelsSize = selectionPixels.pixels.size() * sizeof(uint32_t);
    HGLOBAL clipboardMemory = GlobalAlloc(GMEM_MOVEABLE, headerSize + pixelsSize);
    if (clipboardMemory == nullptr)
    {
        return false;
    }

    auto* dib = static_cast<BYTE*>(GlobalLock(clipboardMemory));
    if (dib == nullptr)
    {
        GlobalFree(clipboardMemory);
        return false;
    }

    auto* header = reinterpret_cast<BITMAPINFOHEADER*>(dib);
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = selectionPixels.width;
    header->biHeight = selectionPixels.height;
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = static_cast<DWORD>(pixelsSize);
    header->biXPelsPerMeter = 0;
    header->biYPelsPerMeter = 0;
    header->biClrUsed = 0;
    header->biClrImportant = 0;

    auto* destination = reinterpret_cast<uint32_t*>(dib + headerSize);
    for (int y = 0; y < selectionPixels.height; ++y)
    {
        const size_t sourceIndex = static_cast<size_t>(y) * selectionPixels.width;
        const size_t destinationIndex = static_cast<size_t>(selectionPixels.height - 1 - y) * selectionPixels.width;
        std::copy_n(selectionPixels.pixels.data() + sourceIndex, selectionPixels.width, destination + destinationIndex);
    }

    GlobalUnlock(clipboardMemory);

    if (!OpenClipboard(owner))
    {
        GlobalFree(clipboardMemory);
        return false;
    }

    EmptyClipboard();
    if (SetClipboardData(CF_DIB, clipboardMemory) == nullptr)
    {
        CloseClipboard();
        GlobalFree(clipboardMemory);
        return false;
    }

    CloseClipboard();
    return true;
}

bool CopyScreenRectToClipboard(HWND owner, const OverlayState& state, const RECT& screenRect)
{
    SelectionPixels selectionPixels;
    if (!GetSelectionPixels(state, screenRect, selectionPixels))
    {
        return false;
    }

    return CopyDibToClipboard(owner, selectionPixels);
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text)
{
    if (!OpenClipboard(owner))
    {
        return false;
    }

    const size_t byteCount = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL clipboardMemory = GlobalAlloc(GMEM_MOVEABLE, byteCount);
    if (clipboardMemory == nullptr)
    {
        CloseClipboard();
        return false;
    }

    void* clipboardData = GlobalLock(clipboardMemory);
    if (clipboardData == nullptr)
    {
        GlobalFree(clipboardMemory);
        CloseClipboard();
        return false;
    }

    std::copy(text.c_str(), text.c_str() + text.size() + 1, static_cast<wchar_t*>(clipboardData));
    GlobalUnlock(clipboardMemory);

    EmptyClipboard();
    if (SetClipboardData(CF_UNICODETEXT, clipboardMemory) == nullptr)
    {
        GlobalFree(clipboardMemory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
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

bool PathExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error);
}

std::filesystem::path MakeUniquePngPath(std::filesystem::path path)
{
    if (path.extension().empty())
    {
        path.replace_extension(L".png");
    }

    if (!PathExists(path))
    {
        return path;
    }

    const std::filesystem::path directory = path.parent_path();
    const std::wstring stem = path.stem().wstring();
    const std::wstring extension = path.extension().wstring();

    for (int index = 1;; ++index)
    {
        std::filesystem::path candidate = directory / (stem + L"_" + std::to_wstring(index) + extension);
        if (!PathExists(candidate))
        {
            return candidate;
        }
    }
}

std::wstring GetDefaultScreenshotFilename()
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

    return filename;
}

std::filesystem::path GetInitialSaveDirectory()
{
    const std::filesystem::path& savedDirectory = Settings::Instance().LastSaveDirectory();
    if (!savedDirectory.empty() && PathExists(savedDirectory))
    {
        return savedDirectory;
    }

    PWSTR picturesPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_DEFAULT, nullptr, &picturesPath)) && picturesPath != nullptr)
    {
        std::filesystem::path result = picturesPath;
        CoTaskMemFree(picturesPath);
        if (PathExists(result))
        {
            return result;
        }
    }

    std::error_code error;
    std::filesystem::path currentPath = std::filesystem::current_path(error);
    return error ? std::filesystem::path{} : currentPath;
}

bool ShowSavePngDialog(std::filesystem::path& selectedPath)
{
    const std::filesystem::path initialDirectory = GetInitialSaveDirectory();
    std::filesystem::path defaultPath = MakeUniquePngPath(initialDirectory / GetDefaultScreenshotFilename());
    wchar_t filePath[MAX_PATH]{};
    wcscpy_s(filePath, defaultPath.wstring().c_str());
    const std::wstring initialDirectoryText = initialDirectory.wstring();

    OPENFILENAMEW openFileName{};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.hwndOwner = nullptr;
    openFileName.lpstrFilter = L"PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0";
    openFileName.lpstrFile = filePath;
    openFileName.nMaxFile = static_cast<DWORD>(sizeof(filePath) / sizeof(filePath[0]));
    openFileName.lpstrInitialDir = initialDirectoryText.c_str();
    openFileName.lpstrDefExt = L"png";
    openFileName.Flags =
        OFN_EXPLORER | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
        OFN_NOTESTFILECREATE | OFN_NODEREFERENCELINKS;

    if (GetSaveFileNameW(&openFileName) == FALSE)
    {
        return false;
    }

    selectedPath = filePath;
    if (selectedPath.extension().empty())
    {
        selectedPath.replace_extension(L".png");
    }

    if (PathExists(selectedPath))
    {
        const int answer = MessageBoxW(
            nullptr,
            L"The selected file already exists. Do you want to replace it?",
            L"PrtSc",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (answer != IDYES)
        {
            return false;
        }
    }

    Settings::Instance().SetLastSaveDirectory(selectedPath.parent_path());
    Settings::Instance().Save();
    return true;
}

bool SaveSelectionPixelsAsPng(const SelectionPixels& selectionPixels, const std::filesystem::path& path)
{
    if (selectionPixels.width <= 0 || selectionPixels.height <= 0 || selectionPixels.pixels.empty())
    {
        return false;
    }

    ULONG_PTR gdiplusToken = 0;
    if (!StartGdiplus(gdiplusToken))
    {
        return false;
    }

    CLSID pngClsid{};
    const bool hasPngEncoder = GetPngEncoderClsid(pngClsid);
    bool saved = false;
    if (hasPngEncoder)
    {
        Gdiplus::Bitmap gdiplusBitmap(
            selectionPixels.width,
            selectionPixels.height,
            selectionPixels.width * static_cast<int>(sizeof(uint32_t)),
            PixelFormat32bppRGB,
            reinterpret_cast<BYTE*>(const_cast<uint32_t*>(selectionPixels.pixels.data())));
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

    CopyScreenRectToClipboard(hwnd, state, screenRect);
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

    std::filesystem::path selectedPath;
    if (!ShowSavePngDialog(selectedPath))
    {
        DestroyWindow(hwnd);
        return;
    }

    SelectionPixels selectionPixels;
    if (GetSelectionPixels(state, screenRect, selectionPixels))
    {
        SaveSelectionPixelsAsPng(selectionPixels, selectedPath);
    }

    DestroyWindow(hwnd);
}

void OcrSelectionToClipboardAndClose(HWND hwnd, OverlayState& state)
{
    if (!state.hasSelection || !IsRectVisible(state.selection))
    {
        return;
    }

    const RECT screenRect = SelectionToScreenRect(hwnd, state.selection);
    state.toolbar.Hide();
    ShowWindow(hwnd, SW_HIDE);

    SelectionPixels selectionPixels;
    HBITMAP bitmap = nullptr;
    if (GetSelectionPixels(state, screenRect, selectionPixels))
    {
        bitmap = CreateBitmapFromSelectionPixels(selectionPixels);
    }

    if (bitmap != nullptr)
    {
        std::wstring text;
        if (OcrEngine::RecognizeBitmapText(bitmap, text) && !text.empty())
        {
            CopyTextToClipboard(hwnd, text);
        }
        else
        {
            MessageBoxW(nullptr, L"OCR did not recognize any text.", L"PrtSc", MB_OK | MB_ICONINFORMATION);
        }

        DeleteObject(bitmap);
    }

    DestroyWindow(hwnd);
}

void ShowColorPicker(HWND hwnd, OverlayState& state)
{
    static COLORREF customColors[16]{};

    CHOOSECOLORW chooseColor{};
    chooseColor.lStructSize = sizeof(chooseColor);
    chooseColor.hwndOwner = hwnd;
    chooseColor.rgbResult = state.annotationColor;
    chooseColor.lpCustColors = customColors;
    chooseColor.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&chooseColor) != FALSE)
    {
        state.annotationColor = chooseColor.rgbResult;
        state.toolbar.SetColor(state.annotationColor);
        Settings::Instance().SetAnnotationColor(state.annotationColor);
        Settings::Instance().Save();
    }
}

bool HideCurrentArrow(OverlayState& state)
{
    if (state.currentArrowIndex < 0 || state.currentArrowIndex >= static_cast<int>(state.arrows.size()))
    {
        return false;
    }

    state.arrows[static_cast<size_t>(state.currentArrowIndex)].isVisible = false;
    ResetArrowUndoState(state);
    state.hasPreviewArrow = false;
    return true;
}

bool ShowNextArrow(OverlayState& state)
{
    const int nextArrowIndex = state.currentArrowIndex + 1;
    if (nextArrowIndex < 0 || nextArrowIndex >= static_cast<int>(state.arrows.size()))
    {
        return false;
    }

    ArrowLine& arrow = state.arrows[static_cast<size_t>(nextArrowIndex)];
    if (arrow.isVisible)
    {
        return false;
    }

    arrow.isVisible = true;
    state.currentArrowIndex = nextArrowIndex;
    state.hasPreviewArrow = false;
    return true;
}

void UndoArrowAndRefresh(HWND hwnd, OverlayState& state)
{
    if (HideCurrentArrow(state))
    {
        RenderOverlay(hwnd, state);
        ShowToolbarForSelection(hwnd, state);
    }
}

void RedoArrowAndRefresh(HWND hwnd, OverlayState& state)
{
    if (ShowNextArrow(state))
    {
        RenderOverlay(hwnd, state);
        ShowToolbarForSelection(hwnd, state);
    }
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

    case kCaptureToolbarOcrMessage:
        if (state != nullptr)
        {
            OcrSelectionToClipboardAndClose(hwnd, *state);
        }
        return 0;

    case kCaptureToolbarUndoArrowMessage:
        if (state != nullptr)
        {
            UndoArrowAndRefresh(hwnd, *state);
        }
        return 0;

    case kCaptureToolbarRedoArrowMessage:
        if (state != nullptr)
        {
            RedoArrowAndRefresh(hwnd, *state);
        }
        return 0;

    case kCaptureToolbarColorMessage:
        if (state != nullptr)
        {
            ShowColorPicker(hwnd, *state);
        }
        return 0;

    case kCaptureToolbarArrowMessage:
        if (state != nullptr && state->hasSelection)
        {
            state->toolbar.Hide();
            state->isArrowToolActive = true;
            state->mode = SelectionMode::None;
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
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
        if (state != nullptr && IsCtrlPressed())
        {
            if (wparam == 'C')
            {
                CopySelectionToClipboardAndClose(hwnd, *state);
                return 0;
            }
            if (wparam == 'S')
            {
                SaveSelectionToFileAndClose(hwnd, *state);
                return 0;
            }
            if (wparam == 'Z')
            {
                UndoArrowAndRefresh(hwnd, *state);
                return 0;
            }
            if (wparam == 'Y')
            {
                RedoArrowAndRefresh(hwnd, *state);
                return 0;
            }
        }
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
            state->previousArrows = state->arrows;
            state->hasPreviewArrow = false;
            state->toolbar.Hide();

            if (state->isArrowToolActive && state->hasSelection)
            {
                if (IsPointInsideRect(point, state->selection))
                {
                    state->mode = SelectionMode::Arrow;
                }
                else
                {
                    state->isMouseDown = false;
                    state->isDragging = false;
                    state->mode = SelectionMode::None;
                    return 0;
                }
            }
            else if (state->hasSelection && IsPointInsideRect(point, state->selection))
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
                state->arrows.clear();
                ResetArrowUndoState(*state);
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (state != nullptr && state->isMouseDown && (wparam & MK_LBUTTON) != 0)
        {
            const POINT current = GetClientPoint(lparam);
            if (state->mode == SelectionMode::Arrow && state->hasSelection)
            {
                POINT arrowEnd = ClampPointToRect(current, state->selection);
                if (arrowEnd.x != state->dragStart.x || arrowEnd.y != state->dragStart.y)
                {
                    state->isDragging = true;
                    state->hasPreviewArrow = true;
                    state->previewArrow = {state->dragStart, arrowEnd, state->annotationColor};
                    RenderOverlayThrottled(hwnd, *state);
                }
            }
            else if (state->mode == SelectionMode::Move && state->hasSelection)
            {
                state->isDragging = true;
                state->selection = MoveRectToPoint(state->previousSelection, current, state->moveOffset, state->size);
                state->arrows = MoveArrows(
                    state->previousArrows,
                    state->selection.left - state->previousSelection.left,
                    state->selection.top - state->previousSelection.top);
                RenderOverlayThrottled(hwnd, *state);
            }
            else if (state->mode == SelectionMode::Create)
            {
                RECT selection = NormalizeRect(state->dragStart, current);

                if (IsRectVisible(selection))
                {
                    state->isDragging = true;
                    state->hasSelection = true;
                    state->selection = selection;
                    RenderOverlayThrottled(hwnd, *state);
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
            const bool wasDrawingArrow = state->mode == SelectionMode::Arrow;
            const bool hadPreviewArrow = state->hasPreviewArrow;
            const ArrowLine completedArrow = state->previewArrow;

            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }

            if (!wasDragging)
            {
                state->selection = state->previousSelection;
                state->arrows = state->previousArrows;
                ResetArrowUndoState(*state);
                state->hasSelection = IsRectVisible(state->selection);
                state->hasPreviewArrow = false;
                RenderOverlay(hwnd, *state);
                if (state->hasSelection)
                {
                    ShowToolbarForSelection(hwnd, *state);
                }
            }
            else
            {
                state->hasSelection = IsRectVisible(state->selection);
                if (wasDrawingArrow && hadPreviewArrow)
                {
                    state->arrows.erase(
                        std::remove_if(
                            state->arrows.begin(),
                            state->arrows.end(),
                            [](const ArrowLine& arrow)
                            {
                                return !arrow.isVisible;
                            }),
                        state->arrows.end());
                    state->arrows.push_back(completedArrow);
                    ResetArrowUndoState(*state);
                    state->hasPreviewArrow = false;
                }
                RenderOverlay(hwnd, *state);
                ShowToolbarForSelection(hwnd, *state);
            }

            state->isMouseDown = false;
            state->isDragging = false;
            state->isArrowToolActive = false;
            state->hasPreviewArrow = false;
            state->mode = SelectionMode::None;
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (state != nullptr)
        {
            state->isMouseDown = false;
            state->isDragging = false;
            state->hasPreviewArrow = false;
            state->mode = SelectionMode::None;
            RenderOverlay(hwnd, *state);
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
    state->annotationColor = Settings::Instance().AnnotationColor();
    if (!CaptureScreenSnapshot(*state, {x, y, x + width, y + height}))
    {
        return false;
    }
    state->hasGdiplus = StartGdiplus(state->gdiplusToken);

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
