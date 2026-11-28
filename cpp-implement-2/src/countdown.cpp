#include "app.h"

namespace wordlock {

void StopCountdown() {
    if (g_countDownWindow && IsWindow(g_countDownWindow)) {
        KillTimer(g_countDownWindow, kCountDownTimerId);
        ShowWindow(g_countDownWindow, SW_HIDE);
    }
}

void StartCountdown() {
    g_countdownSeconds = std::clamp(g_settings.countdownSeconds, 1, 3600);

    if (!g_isQuitting) {
        NotifyActiveViewHide();
    }
    HideSettingsWindows();
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        ShowWindow(g_mainWindow, SW_HIDE);
        SyncChromeFreeze();
    }
    if (g_countDownWindow && IsWindow(g_countDownWindow)) {
        KillTimer(g_countDownWindow, kCountDownTimerId);
        SetTimer(g_countDownWindow, kCountDownTimerId,
                 TestIntervalMs(kTestCountdownTickMsEnv, 1000), nullptr);
        ApplyCountDownPosition(g_countDownWindow);
        ShowWindow(g_countDownWindow, SW_SHOW);
        UpdateWindow(g_countDownWindow);
    }
}

const wchar_t* TaskAsset(TaskType task) {
    switch (task) {
    case TaskType::English: return L"english.html";
    case TaskType::Mcq: return L"mcq.html";
    case TaskType::Japanese: return L"japanese.html";
    case TaskType::Dumbbell: return L"dumbbell.html";
    }
    return L"english.html";
}

void NavigateMainToTask(TaskType task) {
    g_currentTask = task;
    if (!g_mainViewController) {
        return;
    }
    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_mainViewController->get_CoreWebView2(&webView)) && webView) {
        const std::wstring url = AppAssetUrl(TaskAsset(task));
        webView->Navigate(url.c_str());
    }
}

}  // namespace wordlock
