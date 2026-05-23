#include "StartupManager.h"

#include <windows.h>

namespace
{
using GetCurrentPackageFullNameFn = LONG(WINAPI*)(UINT32*, PWSTR);
using StartupIsSupportedFn = BOOL(WINAPI*)();
using StartupIsEnabledFn = BOOL(WINAPI*)(BOOL*);
using StartupSetEnabledFn = BOOL(WINAPI*)(BOOL);

constexpr wchar_t kStartupMsixDllName[] = L"PrtScStartupMsix.dll";

bool IsPackaged()
{
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32 == nullptr)
    {
        return false;
    }

    auto getCurrentPackageFullName =
        reinterpret_cast<GetCurrentPackageFullNameFn>(GetProcAddress(kernel32, "GetCurrentPackageFullName"));
    if (getCurrentPackageFullName == nullptr)
    {
        return false;
    }

    UINT32 length = 0;
    const LONG result = getCurrentPackageFullName(&length, nullptr);
    return result == ERROR_INSUFFICIENT_BUFFER;
}

HMODULE GetStartupMsixModule()
{
    static HMODULE module = []()
    {
        if (!IsPackaged())
        {
            return static_cast<HMODULE>(nullptr);
        }

        return LoadLibraryW(kStartupMsixDllName);
    }();

    return module;
}

template <typename Function>
Function GetStartupMsixFunction(const char* name)
{
    HMODULE module = GetStartupMsixModule();
    if (module == nullptr)
    {
        return nullptr;
    }

    return reinterpret_cast<Function>(GetProcAddress(module, name));
}
}

bool IsStartupManagementAvailable()
{
    auto isSupported = GetStartupMsixFunction<StartupIsSupportedFn>("PrtScStartupIsSupported");
    return isSupported != nullptr && isSupported() != FALSE;
}

bool IsRunAtSystemStartupEnabled()
{
    auto isEnabled = GetStartupMsixFunction<StartupIsEnabledFn>("PrtScStartupIsEnabled");
    if (isEnabled == nullptr)
    {
        return false;
    }

    BOOL enabled = FALSE;
    return isEnabled(&enabled) != FALSE && enabled != FALSE;
}

bool SetRunAtSystemStartup(bool enabled)
{
    auto setEnabled = GetStartupMsixFunction<StartupSetEnabledFn>("PrtScStartupSetEnabled");
    return setEnabled != nullptr && setEnabled(enabled ? TRUE : FALSE) != FALSE;
}
