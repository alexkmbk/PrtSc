#pragma once

#include <windows.h>

class HotkeyManager
{
public:
    HotkeyManager() = default;
    ~HotkeyManager();

    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    bool Register(HWND hwnd);
    void Unregister(HWND hwnd);
    bool IsHotkeyMessage(UINT message, WPARAM wparam) const;

private:
    bool isRegistered_ = false;
};
