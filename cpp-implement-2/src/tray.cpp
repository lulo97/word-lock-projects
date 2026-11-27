#include "app.h"

namespace wordlock {

void ShowTrayMenu(HWND hwnd) {
    POINT cursor{};
    GetCursorPos(&cursor);

    // Required so the menu closes correctly when focus moves elsewhere.
    SetForegroundWindow(hwnd);

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Show App");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTING, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Quit");

    UINT command = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN,
        cursor.x, cursor.y,
        0,
        hwnd,
        nullptr);

    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    switch (command) {
    case ID_TRAY_OPEN:
        ShowMainWindow();
        break;
    case ID_TRAY_SETTING:
        OpenSettingWindow();
        break;
    case ID_TRAY_EXIT:
        // Distinguish an intentional quit from closing the task window, which
        // only returns to the countdown.
        QuitApp();
        break;
    default:
        break;
    }
}

}  // namespace wordlock
