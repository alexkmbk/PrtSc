#pragma once

#include <string>
#include <windows.h>

namespace Gdiplus
{
class Graphics;
}

struct TextAnnotation
{
    POINT position{};
    std::wstring text;
    COLORREF color = RGB(255, 0, 0);
};

void MoveTextAnnotation(TextAnnotation& text, LONG dx, LONG dy);
void DrawTextAnnotation(Gdiplus::Graphics& graphics, const TextAnnotation& text, int offsetX = 0, int offsetY = 0);
