#include "Settings.h"

#include "lib/SimpleIni/SimpleIni.h"

#include <filesystem>
#include <string>
#include <utility>
#include <windows.h>

namespace
{
std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const char* value)
{
    if (value == nullptr || value[0] == '\0')
    {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value, -1, result.data(), size);
    return result;
}
}

const std::filesystem::path Settings::settingsFilePath_ = L"settings.ini";

Settings& Settings::Instance()
{
    static Settings settings;
    return settings;
}

void Settings::Load()
{
    if (!std::filesystem::exists(settingsFilePath_))
    {
        return;
    }

    CSimpleIniA ini;
    ini.SetUnicode();
    if (ini.LoadFile(settingsFilePath_.string().c_str()) < 0)
    {
        return;
    }

    const std::wstring hotkey = Utf8ToWide(ini.GetValue("Hotkeys", "Screenshot", ""));
    if (!hotkey.empty())
    {
        screenshotHotkey_ = hotkey;
    }

    runAtSystemStartup_ = ini.GetBoolValue("General", "RunAtSystemStartup", false);
}

void Settings::Save() const
{
    CSimpleIniA ini;
    ini.SetUnicode();
    ini.SetBoolValue("General", "RunAtSystemStartup", runAtSystemStartup_);
    ini.SetValue("Hotkeys", "Screenshot", WideToUtf8(screenshotHotkey_).c_str());
    ini.SaveFile(settingsFilePath_.string().c_str());
}

const std::wstring& Settings::ScreenshotHotkey() const
{
    return screenshotHotkey_;
}

void Settings::SetScreenshotHotkey(std::wstring hotkey)
{
    screenshotHotkey_ = std::move(hotkey);
}

bool Settings::RunAtSystemStartup() const
{
    return runAtSystemStartup_;
}

void Settings::SetRunAtSystemStartup(bool enabled)
{
    runAtSystemStartup_ = enabled;
}
