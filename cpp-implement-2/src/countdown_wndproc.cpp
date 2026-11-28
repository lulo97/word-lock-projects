#include "app.h"

namespace wordlock {

LRESULT CALLBACK CountDownWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static bool cursorLogged = false;
    switch (message) {
    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
        if (!cursorLogged && IsTestModeEnabled()) {
            AppendTestMessageLog(L"countdown:cursor-sizeall");
            cursorLogged = true;
        }
        return TRUE;

    case WM_NCHITTEST:
        return HTCAPTION;

    case WM_MOVE:
        RememberCountDownPosition(hwnd);
        break;

    case WM_CREATE:
        g_countdownSeconds = std::clamp(g_settings.countdownSeconds, 1, 3600);
        SetTimer(hwnd, kCountDownTimerId,
                 TestIntervalMs(kTestCountdownTickMsEnv, 1000), nullptr);
        return 0;

    case kThemeChangedMessage:
        if (IsTestModeEnabled()) {
            AppendTestMessageLog(L"countdown:theme:" + g_countDownTheme);
        }
        InvalidateRect(hwnd, nullptr, TRUE);
        UpdateWindow(hwnd);
        return 0;

    case WM_PAINT: {
        std::wstring text = std::to_wstring(g_countdownSeconds);
        PaintCenteredText(hwnd, text.c_str(), kCountDownFontHeight,
                          GetThemePalette(g_countDownTheme));
        return 0;
    }

    case WM_TIMER:
        if (wParam == kCountDownTimerId && g_countdownSeconds > 0) {
            --g_countdownSeconds;
            if (g_countdownSeconds == 0) {
                // Countdown finished: switch back to MainWindow.
                ShowMainWindow();
            } else {
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, kCountDownTimerId);
        g_countDownWindow = nullptr;
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace wordlock
