#include "app.h"

namespace wordlock {

void ShowMainWindow() {
    StopCountdown();
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        ShowWindow(g_mainWindow, SW_MAXIMIZE);
        BringWindowToForeground(g_mainWindow);
        SyncChromeFreeze();

        const TaskType task = GetNextTaskType();

        // WebView2 renders reliably only after its parent window has been shown.
        if (!g_mainWebViewInitialized) {
            g_mainWebViewInitialized = true;
            g_currentTask = task;
            InitializeMainWebView(g_mainWindow);
        } else {
            NavigateMainToTask(task);
        }
    }
}

void NotifyActiveViewHide() {
    if (!g_mainViewController) {
        return;
    }
    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_mainViewController->get_CoreWebView2(&webView)) && webView) {
        webView->PostWebMessageAsJson(L"{\"type\":\"app:hide\"}");
    }
}

void ToggleMainWindow() {
    if (g_mainWindow && IsWindow(g_mainWindow) && IsWindowVisible(g_mainWindow)) {
        StartCountdown();
    } else {
        ShowMainWindow();
    }
}

void QuitApp() {
    g_isQuitting = true;
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        DestroyWindow(g_mainWindow);
    } else {
        PostQuitMessage(0);
    }
}

void HideSettingsWindows() {
    if (g_settingWindow && IsWindow(g_settingWindow)) {
        ShowWindow(g_settingWindow, SW_HIDE);
    }
    if (g_ignoreWordsWindow && IsWindow(g_ignoreWordsWindow)) {
        ShowWindow(g_ignoreWordsWindow, SW_HIDE);
    }
    // Closing settings resumes the timer only if the user did not stop the
    // countdown from the tray menu.
    ResumeCountdownForSettings();
}

void UpdateWebViewBounds(HWND hwnd, ComPtr<ICoreWebView2Controller>& controller) {
    if (!controller) {
        return;
    }
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    controller->put_Bounds(bounds);
}

}  // namespace wordlock
