#include "HotkeyManager.h"

#include "Settings.h"

#include <cwctype>
#include <string>
#include <vector>

namespace
{
constexpr int kScreenshotHotkeyId = 1;

std::wstring Trim(std::wstring value)
{
    while (!value.empty() && std::iswspace(value.front()) != 0)
    {
        value.erase(value.begin());
    }

    while (!value.empty() && std::iswspace(value.back()) != 0)
    {
        value.pop_back();
    }

    return value;
}

std::wstring ToLower(std::wstring value)
{
    for (wchar_t& ch : value)
    {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return value;
}

std::vector<std::wstring> SplitHotkey(const std::wstring& hotkey)
{
    std::vector<std::wstring> parts;
    size_t start = 0;

    while (start <= hotkey.size())
    {
        const size_t separator = hotkey.find(L'+', start);
        const size_t end = separator == std::wstring::npos ? hotkey.size() : separator;
        parts.push_back(Trim(hotkey.substr(start, end - start)));

        if (separator == std::wstring::npos)
        {
            break;
        }

        start = separator + 1;
    }

    return parts;
}

bool TryParseMainKey(const std::wstring& key, UINT& virtualKey)
{
    const std::wstring normalized = ToLower(key);

    if (normalized == L"prtscr" || normalized == L"printscreen" || normalized == L"print screen")
    {
        virtualKey = VK_SNAPSHOT;
        return true;
    }
    if (normalized == L"esc" || normalized == L"escape")
    {
        virtualKey = VK_ESCAPE;
        return true;
    }
    if (normalized == L"enter")
    {
        virtualKey = VK_RETURN;
        return true;
    }
    if (normalized == L"space")
    {
        virtualKey = VK_SPACE;
        return true;
    }
    if (normalized == L"tab")
    {
        virtualKey = VK_TAB;
        return true;
    }

    if (normalized.starts_with(L"f") && normalized.size() >= 2)
    {
        int number = 0;
        for (size_t i = 1; i < normalized.size(); ++i)
        {
            if (normalized[i] < L'0' || normalized[i] > L'9')
            {
                return false;
            }

            number = number * 10 + (normalized[i] - L'0');
        }

        if (number >= 1 && number <= 24)
        {
            virtualKey = static_cast<UINT>(VK_F1 + number - 1);
            return true;
        }
    }

    if (normalized.size() == 1)
    {
        const wchar_t ch = normalized[0];
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9'))
        {
            virtualKey = static_cast<UINT>(std::towupper(ch));
            return true;
        }
    }

    return false;
}

bool TryParseHotkey(const std::wstring& hotkey, UINT& modifiers, UINT& virtualKey)
{
    modifiers = MOD_NOREPEAT;
    virtualKey = 0;

    for (const std::wstring& rawPart : SplitHotkey(hotkey))
    {
        const std::wstring part = ToLower(rawPart);
        if (part.empty())
        {
            continue;
        }

        if (part == L"ctrl" || part == L"control")
        {
            modifiers |= MOD_CONTROL;
        }
        else if (part == L"alt")
        {
            modifiers |= MOD_ALT;
        }
        else if (part == L"shift")
        {
            modifiers |= MOD_SHIFT;
        }
        else if (part == L"win" || part == L"windows")
        {
            modifiers |= MOD_WIN;
        }
        else if (!TryParseMainKey(part, virtualKey))
        {
            return false;
        }
    }

    return virtualKey != 0;
}
}

HotkeyManager::~HotkeyManager() = default;

bool HotkeyManager::Register(HWND hwnd)
{
    Unregister(hwnd);

    UINT modifiers = 0;
    UINT virtualKey = 0;
    if (!TryParseHotkey(Settings::Instance().ScreenshotHotkey(), modifiers, virtualKey))
    {
        return false;
    }

    isRegistered_ = RegisterHotKey(hwnd, kScreenshotHotkeyId, modifiers, virtualKey) != FALSE;
    return isRegistered_;
}

void HotkeyManager::Unregister(HWND hwnd)
{
    if (!isRegistered_)
    {
        return;
    }

    UnregisterHotKey(hwnd, kScreenshotHotkeyId);
    isRegistered_ = false;
}

bool HotkeyManager::IsHotkeyMessage(UINT message, WPARAM wparam) const
{
    return message == WM_HOTKEY && wparam == kScreenshotHotkeyId;
}
