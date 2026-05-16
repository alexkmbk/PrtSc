#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

class Settings
{
public:
    static Settings& Instance();

    void Load();
    void Save() const;

    const std::wstring& ScreenshotHotkey() const;
    void SetScreenshotHotkey(std::wstring hotkey);

    bool RunAtSystemStartup() const;
    void SetRunAtSystemStartup(bool enabled);

    COLORREF AnnotationColor() const;
    void SetAnnotationColor(COLORREF color);

    const std::filesystem::path& LastSaveDirectory() const;
    void SetLastSaveDirectory(std::filesystem::path directory);

private:
    Settings() = default;

    static const std::filesystem::path settingsFilePath_;

    std::wstring screenshotHotkey_ = L"Ctrl+PrtScr";
    bool runAtSystemStartup_ = false;
    COLORREF annotationColor_ = RGB(255, 0, 0);
    std::filesystem::path lastSaveDirectory_;
};
