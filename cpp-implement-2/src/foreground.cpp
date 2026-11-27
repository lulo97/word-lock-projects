#include "app.h"

namespace wordlock {

void BringWindowToForeground(HWND hwnd) {
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread = GetWindowThreadProcessId(foreground, nullptr);
    const DWORD currentThread = GetCurrentThreadId();
    const bool attached = foregroundThread != 0 && foregroundThread != currentThread &&
                          AttachThreadInput(currentThread, foregroundThread, TRUE);

    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    if (attached) {
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
}

}  // namespace wordlock
