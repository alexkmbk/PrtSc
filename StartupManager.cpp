#include "StartupManager.h"

#include <string>
#include <windows.h>

namespace
{
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"PrtSc";

std::wstring GetCurrentExecutableCommand()
{
    std::wstring path(MAX_PATH, L'\0');

    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
        {
            return {};
        }

        if (length < path.size() - 1)
        {
            path.resize(length);
            break;
        }

        path.resize(path.size() * 2);
    }

    return L"\"" + path + L"\"";
}
}

bool SetRunAtSystemStartup(bool enabled)
{
    HKEY runKey = nullptr;
    const LSTATUS openStatus = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &runKey,
        nullptr);

    if (openStatus != ERROR_SUCCESS)
    {
        return false;
    }

    LSTATUS status = ERROR_SUCCESS;
    if (enabled)
    {
        const std::wstring command = GetCurrentExecutableCommand();
        if (command.empty())
        {
            RegCloseKey(runKey);
            return false;
        }

        status = RegSetValueExW(
            runKey,
            kRunValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    }
    else
    {
        status = RegDeleteValueW(runKey, kRunValueName);
        if (status == ERROR_FILE_NOT_FOUND)
        {
            status = ERROR_SUCCESS;
        }
    }

    RegCloseKey(runKey);
    return status == ERROR_SUCCESS;
}
