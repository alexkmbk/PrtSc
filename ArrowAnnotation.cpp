#include "ArrowAnnotation.h"

#include <cmath>
#include <objidl.h>
#include <gdiplus.h>

namespace
{
constexpr float kArrowWidth = 4.0F;
constexpr float kArrowHeadLength = 16.0F;
constexpr float kArrowHeadAngleRadians = 0.65F;

Gdiplus::Color ToGdiplusColor(COLORREF color)
{
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}
}

void MoveArrow(ArrowAnnotation& arrow, LONG dx, LONG dy)
{
    arrow.start.x += dx;
    arrow.start.y += dy;
    arrow.end.x += dx;
    arrow.end.y += dy;
}

void DrawArrow(Gdiplus::Graphics& graphics, const ArrowAnnotation& arrow, int offsetX, int offsetY)
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
