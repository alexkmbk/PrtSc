#pragma once

#include <filesystem>
#include <string>

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

private:
    Settings() = default;

    static const std::filesystem::path settingsFilePath_;

    std::wstring screenshotHotkey_ = L"Ctrl+PrtScr";
    bool runAtSystemStartup_ = false;
};
