#include "Settings.h"

#include "lib/SimpleIni/SimpleIni.h"

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <windows.h>
#include <shlobj_core.h>

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

std::string ColorToString(COLORREF color)
{
    char buffer[8]{};
    sprintf_s(buffer, "%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buffer;
}

bool TryParseColor(const char* value, COLORREF& color)
{
    if (value == nullptr)
    {
        return false;
    }

    std::string text = value;
    if (text.starts_with("#"))
    {
        text.erase(text.begin());
    }

    if (text.size() != 6)
    {
        return false;
    }

    unsigned int rgb = 0;
    std::istringstream stream(text);
    stream >> std::hex >> rgb;
    if (stream.fail())
    {
        return false;
    }

    color = RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    return true;
}

std::filesystem::path GetLocalAppDataPath()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, static_cast<DWORD>(std::size(buffer)));
    if (length > 0 && length < std::size(buffer))
    {
        return buffer;
    }

    PWSTR localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppData)) &&
        localAppData != nullptr)
    {
        std::filesystem::path path = localAppData;
        CoTaskMemFree(localAppData);
        return path;
    }

    const DWORD userProfileLength =
        GetEnvironmentVariableW(L"USERPROFILE", buffer, static_cast<DWORD>(std::size(buffer)));
    if (userProfileLength > 0 && userProfileLength < std::size(buffer))
    {
        return std::filesystem::path(buffer) / L"AppData" / L"Local";
    }

    return std::filesystem::current_path();
}

std::filesystem::path GetSettingsFilePath()
{
    return GetLocalAppDataPath() / L"PrtSc" / L"settings.ini";
}
}

const std::filesystem::path Settings::settingsFilePath_ = GetSettingsFilePath();

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
    if (ini.LoadFile(settingsFilePath_.c_str()) < 0)
    {
        return;
    }

    const std::wstring hotkey = Utf8ToWide(ini.GetValue("Hotkeys", "Screenshot", ""));
    if (!hotkey.empty())
    {
        screenshotHotkey_ = hotkey;
    }

    runAtSystemStartup_ = ini.GetBoolValue("General", "RunAtSystemStartup", false);

    COLORREF color = annotationColor_;
    if (TryParseColor(ini.GetValue("Annotation", "Color", ""), color))
    {
        annotationColor_ = color;
    }

    const std::wstring lastSaveDirectory = Utf8ToWide(ini.GetValue("Save", "LastDirectory", ""));
    if (!lastSaveDirectory.empty())
    {
        lastSaveDirectory_ = lastSaveDirectory;
    }
}

void Settings::Save() const
{
    std::error_code error;
    std::filesystem::create_directories(settingsFilePath_.parent_path(), error);

    CSimpleIniA ini;
    ini.SetUnicode();
    ini.SetBoolValue("General", "RunAtSystemStartup", runAtSystemStartup_);
    ini.SetValue("Hotkeys", "Screenshot", WideToUtf8(screenshotHotkey_).c_str());
    ini.SetValue("Annotation", "Color", ColorToString(annotationColor_).c_str());
    ini.SetValue("Save", "LastDirectory", WideToUtf8(lastSaveDirectory_.wstring()).c_str());
    ini.SaveFile(settingsFilePath_.c_str());
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

COLORREF Settings::AnnotationColor() const
{
    return annotationColor_;
}

void Settings::SetAnnotationColor(COLORREF color)
{
    annotationColor_ = color;
}

const std::filesystem::path& Settings::LastSaveDirectory() const
{
    return lastSaveDirectory_;
}

void Settings::SetLastSaveDirectory(std::filesystem::path directory)
{
    lastSaveDirectory_ = std::move(directory);
}
