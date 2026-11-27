#include "app.h"

namespace wordlock {

UINT SystemDpi() {
    UINT dpi = GetDpiForSystem();
    return dpi == 0 ? USER_DEFAULT_SCREEN_DPI : dpi;
}

HWND CreateMainWindow(HINSTANCE hInstance, const wchar_t* className, UINT dpi) {
    RECT rect{0, 0, ScaleForDpi(kClientWidth, dpi), ScaleForDpi(kClientHeight, dpi)};
    AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
    HWND hwnd = CreateWindowExW(0, className, kWindowTitle,
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) {
        MessageBoxW(nullptr, L"CreateWindowExW failed.", L"Error", MB_ICONERROR | MB_OK);
    }
    return hwnd;
}

void CreateCountDownWindow(HINSTANCE hInstance, const wchar_t* className, UINT dpi) {
    const int width = ScaleForDpi(kCountDownWidth, dpi);
    const int height = ScaleForDpi(kCountDownHeight, dpi);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY), &info)) {
        SetRect(&info.rcWork, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                GetSystemMetrics(SM_CYSCREEN));
    }
    const int margin = ScaleForDpi(kCountDownMargin, dpi);
    g_countDownWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, className, kCountDownTitle, WS_POPUP,
        info.rcWork.right - width - margin, info.rcWork.bottom - height - margin,
        width, height, nullptr, nullptr, hInstance, nullptr);
    if (g_countDownWindow) {
        SetLayeredWindowAttributes(g_countDownWindow, 0, kCountDownOpacity, LWA_ALPHA);
        RememberCountDownPosition(g_countDownWindow);
    } else {
        MessageBoxW(nullptr, L"CreateWindowExW for CountDownWindow failed.",
                    L"Error", MB_ICONERROR | MB_OK);
    }
}

}  // namespace wordlock
