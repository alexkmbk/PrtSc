#include "TextAnnotation.h"

#include <objidl.h>
#include <gdiplus.h>

namespace
{
constexpr float kTextFontSize = 24.0F;
constexpr float kTextLayoutWidth = 2000.0F;
constexpr float kTextLayoutHeight = 2000.0F;

Gdiplus::Color ToGdiplusColor(COLORREF color)
{
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}
}

void MoveTextAnnotation(TextAnnotation& text, LONG dx, LONG dy)
{
    text.position.x += dx;
    text.position.y += dy;
}

void DrawTextAnnotation(Gdiplus::Graphics& graphics, const TextAnnotation& text, int offsetX, int offsetY)
{
    if (text.text.empty())
    {
        return;
    }

    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, kTextFontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(ToGdiplusColor(text.color));
    Gdiplus::StringFormat format;
    format.SetFormatFlags(Gdiplus::StringFormatFlagsNoClip);

    Gdiplus::RectF layout(
        static_cast<Gdiplus::REAL>(text.position.x - offsetX),
        static_cast<Gdiplus::REAL>(text.position.y - offsetY),
        kTextLayoutWidth,
        kTextLayoutHeight);

    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics.DrawString(text.text.c_str(), -1, &font, layout, &format, &brush);
}
