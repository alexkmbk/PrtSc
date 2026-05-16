#include "OcrEngine.h"

#include <filesystem>
#include <string>
#include <objbase.h>

namespace
{
using RecognizeBitmapTextFn = BOOL(WINAPI*)(HBITMAP, PWSTR*);

std::filesystem::path GetOcrDllPath()
{
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return L"PrtScOcr.dll";
    }

    return std::filesystem::path(modulePath).parent_path() / L"PrtScOcr.dll";
}

RecognizeBitmapTextFn LoadRecognizeBitmapText()
{
    static HMODULE ocrModule = LoadLibraryW(GetOcrDllPath().c_str());
    if (ocrModule == nullptr)
    {
        return nullptr;
    }

    static auto recognizeBitmapText = reinterpret_cast<RecognizeBitmapTextFn>(
        GetProcAddress(ocrModule, "PrtScOcrRecognizeBitmapText"));
    return recognizeBitmapText;
}
}

namespace OcrEngine
{
bool RecognizeBitmapText(HBITMAP bitmap, std::wstring& text)
{
    text.clear();

    RecognizeBitmapTextFn recognizeBitmapText = LoadRecognizeBitmapText();
    if (recognizeBitmapText == nullptr)
    {
        return false;
    }

    PWSTR recognizedText = nullptr;
    if (recognizeBitmapText(bitmap, &recognizedText) == FALSE || recognizedText == nullptr)
    {
        if (recognizedText != nullptr)
        {
            CoTaskMemFree(recognizedText);
        }
        return false;
    }

    text = recognizedText;
    CoTaskMemFree(recognizedText);
    return true;
}
}
