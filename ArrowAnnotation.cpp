#include "ArrowAnnotation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <objidl.h>
#include <gdiplus.h>

namespace
{
constexpr wchar_t kArrowClassName[] = L"PrtScArrowAnnotation";
constexpr int kArrowPadding = 24;
constexpr int kMinimumWindowSize = 8;
constexpr float kArrowWidth = 4.0F;
constexpr float kArrowHeadLength = 16.0F;
constexpr float kArrowHeadAngleRadians = 0.65F;

struct ArrowRenderInfo
{
    RECT windowRect{};
    POINT localStart{};
    POINT localEnd{};
};

LRESULT CALLBACK ArrowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool RegisterArrowClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ArrowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kArrowClassName;

    if (RegisterClassExW(&windowClass) != 0)
    {
        return true;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

ArrowRenderInfo BuildRenderInfo(POINT screenStart, POINT screenEnd)
{
    const int left = std::min(screenStart.x, screenEnd.x) - kArrowPadding;
    const int top = std::min(screenStart.y, screenEnd.y) - kArrowPadding;
    const int right = std::max(screenStart.x, screenEnd.x) + kArrowPadding;
    const int bottom = std::max(screenStart.y, screenEnd.y) + kArrowPadding;

    ArrowRenderInfo info{};
    info.windowRect = {
        left,
        top,
        std::max(right, left + kMinimumWindowSize),
        std::max(bottom, top + kMinimumWindowSize),
    };
    info.localStart = {screenStart.x - info.windowRect.left, screenStart.y - info.windowRect.top};
    info.localEnd = {screenEnd.x - info.windowRect.left, screenEnd.y - info.windowRect.top};
    return info;
}

Gdiplus::Color ToGdiplusColor(COLORREF color)
{
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

void DrawArrow(Gdiplus::Graphics& graphics, POINT start, POINT end, COLORREF color)
{
    const float startX = static_cast<float>(start.x);
    const float startY = static_cast<float>(start.y);
    const float endX = static_cast<float>(end.x);
    const float endY = static_cast<float>(end.y);
    const float dx = endX - startX;
    const float dy = endY - startY;
    const float length = std::sqrt((dx * dx) + (dy * dy));
    if (length < 2.0F)
    {
        return;
    }

    Gdiplus::Pen pen(ToGdiplusColor(color), kArrowWidth);
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

void RenderArrow(HWND hwnd, const ArrowRenderInfo& info, COLORREF color)
{
    Gdiplus::GdiplusStartupInput startupInput{};
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
    {
        return;
    }

    const int width = info.windowRect.right - info.windowRect.left;
    const int height = info.windowRect.bottom - info.windowRect.top;

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
    if (bitmap != nullptr && bitmapBits != nullptr)
    {
        std::fill_n(static_cast<uint32_t*>(bitmapBits), static_cast<size_t>(width) * height, 0x00000000);

        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
        {
            Gdiplus::Graphics graphics(memoryDc);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            DrawArrow(graphics, info.localStart, info.localEnd, color);
        }

        POINT windowPoint{info.windowRect.left, info.windowRect.top};
        POINT sourcePoint{};
        SIZE size{width, height};

        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(hwnd, screenDc, &windowPoint, &size, memoryDc, &sourcePoint, 0, &blend, ULW_ALPHA);
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
    }

    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    Gdiplus::GdiplusShutdown(gdiplusToken);
}
}

ArrowAnnotation::~ArrowAnnotation()
{
    Hide();
}

bool ArrowAnnotation::Show(HWND owner, POINT screenStart, POINT screenEnd, COLORREF color)
{
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    if (!RegisterArrowClass(instance))
    {
        return false;
    }

    if (hwnd_ == nullptr || !IsWindow(hwnd_))
    {
        hwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
            kArrowClassName,
            L"PrtSc Arrow Annotation",
            WS_POPUP,
            0,
            0,
            kMinimumWindowSize,
            kMinimumWindowSize,
            nullptr,
            nullptr,
            instance,
            nullptr);

        if (hwnd_ == nullptr)
        {
            return false;
        }

        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    }

    const ArrowRenderInfo info = BuildRenderInfo(screenStart, screenEnd);
    RenderArrow(hwnd_, info, color);
    return true;
}

void ArrowAnnotation::Hide()
{
    if (hwnd_ != nullptr && IsWindow(hwnd_))
    {
        DestroyWindow(hwnd_);
    }

    hwnd_ = nullptr;
}
