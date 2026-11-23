#pragma once

#include <windows.h>
#include <string>

// Apply an icon to a window. This updates the title bar, taskbar,
// Alt+Tab and the icon used by set_tray().
//
// If iconPath is empty, the first available icon is used from:
//   1. assets\icon.ico next to the executable (or one folder above it)
//   2. assets\icon.ico in the current working directory
//   3. resource ID 1 embedded in the executable
//
// The embedded resource is what makes the icon keep working when the
// executable is built and moved away from the source tree.
//
// Returns true when an icon was successfully applied.
bool set_icon(HWND hwnd, const std::wstring& iconPath = L"");

// Apply the icon embedded in an executable/module resource.
// The default resource ID is 1 (the icon added by app.rc).
bool set_icon_from_resource(
    HWND hwnd,
    int resourceId = 1,
    HINSTANCE instance = nullptr
);

// Load an icon from a .ico file. Relative paths are resolved against the
// executable directory first, then the current working directory.
// The returned icon is owned by the caller and must be destroyed with
// DestroyIcon().
HICON load_icon(const std::wstring& iconPath, int width = 0, int height = 0);
