#pragma once

#include <windows.h>
#include <shellapi.h>

class SysTrayIcon
{
public:
    SysTrayIcon() = default;
    ~SysTrayIcon();

    SysTrayIcon(const SysTrayIcon&) = delete;
    SysTrayIcon& operator=(const SysTrayIcon&) = delete;

    bool Initialize(HWND hwnd);
    void Shutdown();

    bool HandleMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
    void ShowContextMenu(HWND hwnd) const;

    NOTIFYICONDATAW notifyIconData_{};
    HICON icon_ = nullptr;
    bool isInitialized_ = false;
};
