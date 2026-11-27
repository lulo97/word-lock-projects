#include "app.h"

namespace wordlock {

int RunMessageLoop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    UnfreezeChromeOnExit();
    return static_cast<int>(message.wParam);
}

int Run(HINSTANCE hInstance, int nCmdShow) {
    if (!AcquireSingleInstanceLock()) {
        ResumeHeldChromeProcesses();
        return 0;
    }
    g_hInstance = hInstance;
    const bool testOpenSettings = WantsTestSettingsOpen();
    const bool testOpenDumbbell = WantsTestDumbbellOpen();
    LoadSettings();
    if (testOpenDumbbell) {
        g_settings.tasks.englishWord = false;
        g_settings.tasks.multipleChoice = false;
        g_settings.tasks.japaneseWord = false;
        g_settings.tasks.dumbbellWorkout = true;
    }
    InitializeDatabase();
    SyncAutoStartOnStartup();

    const bool comInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        SetProcessDPIAware();
    }
    if (!IsTestModeEnabled()) {
        ResumeAllChromeProcesses();
    }

    const WindowClasses classes{
        WindowClassForCurrentProcess(kWindowClass),
        WindowClassForCurrentProcess(kCountDownClass),
        WindowClassForCurrentProcess(kSettingClass),
        WindowClassForCurrentProcess(kIgnoreClass)};
    if (!RegisterAllWindowClasses(hInstance, classes)) {
        return 1;
    }

    const UINT dpi = SystemDpi();
    HWND hwnd = CreateMainWindow(hInstance, classes.main.c_str(), dpi);
    if (!hwnd) {
        return 1;
    }
    CreateCountDownWindow(hInstance, classes.countDown.c_str(), dpi);

    SyncChromeFreeze();
    (void)nCmdShow;
    ShowWindow(hwnd, SW_HIDE);
    if (g_countDownWindow) {
        ShowWindow(g_countDownWindow, SW_SHOW);
        UpdateWindow(g_countDownWindow);
    }
    if (testOpenSettings) {
        OpenSettingWindow();
    }
    if (testOpenDumbbell) {
        ShowMainWindow();
    }

    const int result = RunMessageLoop();
    if (comInitialized) {
        CoUninitialize();
    }
    return result;
}

}  // namespace wordlock
