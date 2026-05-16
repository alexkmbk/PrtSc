#pragma once

#include <windows.h>

#include <string>

namespace OcrEngine
{
bool RecognizeBitmapText(HBITMAP bitmap, std::wstring& text);
}
