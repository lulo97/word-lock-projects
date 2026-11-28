#include "app.h"

namespace wordlock {

void OpenSettingWindow() {
    // Keep the countdown window visible, but stop its timer while settings are
    // open. This is independent of a tray pause.
    PauseCountdownForSettings();
    if (g_settingWindow && IsWindow(g_settingWindow)) {
        SendSettingsToSettingsWebView();
        ShowWindow(g_settingWindow, SW_SHOW);
        SetForegroundWindow(g_settingWindow);
        return;
    }

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) {
        dpi = USER_DEFAULT_SCREEN_DPI;
    }

    const std::wstring settingClass = WindowClassForCurrentProcess(kSettingClass);
    g_settingWindow = CreateWindowExW(
        0,
        settingClass.c_str(),
        kSettingTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        ScaleForDpi(kSettingWindowWidth, dpi),
        ScaleForDpi(kSettingWindowHeight, dpi),
        g_mainWindow,
        nullptr,
        g_hInstance,
        nullptr);

    if (g_settingWindow) {
        ShowWindow(g_settingWindow, SW_SHOW);
        UpdateWindow(g_settingWindow);
    } else {
        ResumeCountdownForSettings();
    }
}

LRESULT CALLBACK SettingWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        InitializeWebView(hwnd, L"settings.html", &g_webViewController);
        return 0;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_SIZE:
        UpdateWebViewBounds(hwnd, g_webViewController);
        break;

    case WM_CLOSE:
        // Closing without confirming reverts a live theme preview.
        BroadcastTheme(g_settings.theme);
        HideSettingsWindows();
        return 0;

    case WM_DESTROY:
        UnregisterWebViewBinding(g_webViewController.Get());
        g_webViewController.Reset();
        g_settingWindow = nullptr;
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace wordlock
