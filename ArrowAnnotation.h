#pragma once

#include <windows.h>

namespace Gdiplus
{
class Graphics;
}

struct ArrowAnnotation
{
    POINT start{};
    POINT end{};
    COLORREF color = RGB(255, 0, 0);
};

void MoveArrow(ArrowAnnotation& arrow, LONG dx, LONG dy);
void DrawArrow(Gdiplus::Graphics& graphics, const ArrowAnnotation& arrow, int offsetX = 0, int offsetY = 0);
