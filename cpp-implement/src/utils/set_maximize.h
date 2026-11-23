#pragma once

#include <windows.h>

// Maximize a window the same way the title-bar maximize button does.
// This respects the monitor's work area (taskbar stays visible), so it is
// normal maximize mode, not borderless/fullscreen mode.
void set_maximize(HWND hwnd);
