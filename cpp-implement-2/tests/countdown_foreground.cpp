#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

LRESULT CALLBACK DistractorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void PumpMessages(DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ForceForeground(HWND hwnd) {
    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread = GetWindowThreadProcessId(foreground, nullptr);
    const DWORD currentThread = GetCurrentThreadId();
    const bool attached = foregroundThread != 0 && foregroundThread != currentThread &&
                          AttachThreadInput(currentThread, foregroundThread, TRUE);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    if (attached) {
        SetFocus(hwnd);
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
}

HWND CreateDistractor() {
    const std::wstring className =
        L"CountdownForegroundDistractor" + std::to_wstring(GetCurrentProcessId());
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = DistractorProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className.c_str();
    REQUIRE(RegisterClassW(&windowClass) != 0);

    HWND hwnd = CreateWindowExW(0, className.c_str(), L"Countdown Distractor",
                                WS_OVERLAPPEDWINDOW, 80, 80, 480, 320,
                                nullptr, nullptr, windowClass.hInstance, nullptr);
    REQUIRE(hwnd != nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return hwnd;
}

}  // namespace

TEST_CASE("Countdown timeout brings the main window to the foreground",
          "[countdown][foreground][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp::Options options;
    options.env = {{L"WORDLOCK_TEST_COUNTDOWN_SECONDS", L"2"}};
    options.prefix = L"PomodoroLockCountdownForeground";
    TestApp app(options);

    HWND countdown = WaitForWindow(app.countdownClass, true, 3000);
    REQUIRE(countdown != nullptr);
    const HWND mainWindow = app.Main();
    REQUIRE(mainWindow != nullptr);
    REQUIRE(!IsWindowVisible(mainWindow));

    const HWND distractor = CreateDistractor();
    ForceForeground(distractor);
    PumpMessages(100);
    REQUIRE(GetForegroundWindow() == distractor);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(4000);
    while (std::chrono::steady_clock::now() < deadline && !IsWindowVisible(mainWindow)) {
        PumpMessages(20);
    }
    REQUIRE(IsWindowVisible(mainWindow));
    REQUIRE(!IsWindowVisible(countdown));
    PumpMessages(100);

    INFO("Foreground after countdown: " << GetForegroundWindow() << " main: " << mainWindow);
    REQUIRE(GetForegroundWindow() == mainWindow);

    DestroyWindow(distractor);
    app.Quit();
}
