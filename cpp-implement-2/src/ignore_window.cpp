#include "app.h"

namespace wordlock {

void OpenIgnoreWordsWindow() {
    if (g_ignoreWordsWindow && IsWindow(g_ignoreWordsWindow)) {
        ShowWindow(g_ignoreWordsWindow, SW_SHOW);
        SetForegroundWindow(g_ignoreWordsWindow);
        return;
    }

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) {
        dpi = USER_DEFAULT_SCREEN_DPI;
    }

    const std::wstring ignoreClass = WindowClassForCurrentProcess(kIgnoreClass);
    g_ignoreWordsWindow = CreateWindowExW(
        0,
        ignoreClass.c_str(),
        kIgnoreTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        ScaleForDpi(kIgnoreWindowWidth, dpi),
        ScaleForDpi(kIgnoreWindowHeight, dpi),
        g_settingWindow,
        nullptr,
        g_hInstance,
        nullptr);

    if (g_ignoreWordsWindow) {
        ShowWindow(g_ignoreWordsWindow, SW_SHOW);
        UpdateWindow(g_ignoreWordsWindow);
    }
}

void HideIgnoreWordsWindow() {
    if (g_ignoreWordsWindow && IsWindow(g_ignoreWordsWindow)) {
        ShowWindow(g_ignoreWordsWindow, SW_HIDE);
    }
}

LRESULT CALLBACK IgnoreWordsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        InitializeWebView(hwnd, L"ignoreWords.html", &g_ignoreWordsController);
        return 0;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_SIZE:
        UpdateWebViewBounds(hwnd, g_ignoreWordsController);
        break;

    case WM_CLOSE:
        HideIgnoreWordsWindow();
        return 0;

    case WM_DESTROY:
        UnregisterWebViewBinding(g_ignoreWordsController.Get());
        g_ignoreWordsController.Reset();
        g_ignoreWordsWindow = nullptr;
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace wordlock
