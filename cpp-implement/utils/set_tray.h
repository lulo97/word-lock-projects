#pragma once

#include <windows.h>
#include <string>

// Callback message sent by the tray icon to the window.
// The window procedure should forward it to handle_tray_message().
constexpr UINT WM_TRAY_CALLBACK = WM_APP + 101;

// Add the window to the Windows notification area (system tray).
// A null icon uses the window icon, then the executable icon, then a
// default application icon. An empty tooltip uses the window title.
bool set_tray(
    HWND hwnd,
    HICON icon = nullptr,
    const std::wstring& tooltip = L""
);

// Remove the window's tray icon.
void remove_tray(HWND hwnd);

// Handle a WM_TRAY_CALLBACK message.
// Left-click or right-click opens the tray menu (Open App / Exit).
void handle_tray_message(HWND hwnd, LPARAM lParam);
