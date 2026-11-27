#include "app.h"

namespace wordlock {

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_mainWindow = hwnd;

        g_trayIcon.cbSize = sizeof(g_trayIcon);
        g_trayIcon.hWnd = hwnd;
        g_trayIcon.uID = kTrayIconId;
        g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_trayIcon.uCallbackMessage = kTrayMessage;
        g_trayIcon.hIcon = LoadAppIcon(16, 16);
        wcscpy_s(g_trayIcon.szTip, kTrayTip);

        if (!Shell_NotifyIconW(NIM_ADD, &g_trayIcon)) {
            MessageBoxW(hwnd, L"Failed to add the tray icon.", L"Error", MB_ICONERROR | MB_OK);
        }
        return 0;

    case kTrayMessage:
        switch (static_cast<UINT>(lParam)) {
        case WM_RBUTTONUP:
            ShowTrayMenu(hwnd);
            return 0;
        case WM_LBUTTONUP:
            ToggleMainWindow();
            return 0;
        default:
            return 0;
        }

    case kTestQuitMessage:
        if (IsTestModeEnabled()) {
            QuitApp();
            return 0;
        }
        break;

    case WM_SIZE:
        // Minimize goes to the tray; use the tray icon or menu to bring it back.
        if (wParam == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
            SyncChromeFreeze();
            return 0;
        }
        UpdateWebViewBounds(hwnd, g_mainViewController);
        break;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_TIMER:
        if (wParam == kChromeFreezeTimerId && g_settings.chromeFreeze && IsWindowVisible(hwnd)) {
            FreezeChromeProcesses();
        }
        return 0;

    case WM_CLOSE:
        if (g_isQuitting) {
            QuitApp();
        } else {
            // Closing MainWindow returns to a fresh 60s countdown instead of
            // ending the process; the tray icon keeps the app alive.
            StartCountdown();
        }
        return 0;

    case WM_DESTROY:
        if (g_ignoreWordsWindow && IsWindow(g_ignoreWordsWindow)) {
            DestroyWindow(g_ignoreWordsWindow);
        }
        if (g_settingWindow && IsWindow(g_settingWindow)) {
            DestroyWindow(g_settingWindow);
        }
        if (g_countDownWindow && IsWindow(g_countDownWindow)) {
            DestroyWindow(g_countDownWindow);
        }
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
        UnregisterWebViewBinding(g_mainViewController.Get());
        g_mainViewController.Reset();
        UnfreezeChromeOnExit();
        g_mainWindow = nullptr;
        g_countDownWindow = nullptr;
        g_ignoreWordsWindow = nullptr;
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace wordlock
