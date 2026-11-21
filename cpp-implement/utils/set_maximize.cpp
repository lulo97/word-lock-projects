#include "set_maximize.h"

void set_maximize(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd))
        return;

    // If the window is minimized, restore it first so SW_MAXIMIZE applies
    // to a normal window state.
    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);

    // Same result as clicking the maximize button: the window fills the
    // monitor work area while keeping its normal frame and taskbar visible.
    ShowWindow(hwnd, SW_MAXIMIZE);
}
