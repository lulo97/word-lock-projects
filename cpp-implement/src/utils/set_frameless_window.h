#pragma once

#include <windows.h>

// Remove the title bar, window borders and system buttons
// (minimize / maximize / close) so the window is frameless.
//
// The window keeps its current position, size and visibility.
void set_frameless_window(HWND hwnd);

// Restore the normal Windows frame (title bar and system buttons).
void set_framed_window(HWND hwnd);
