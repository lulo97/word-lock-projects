#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

TestApp::Options SettingsCountdownOptions() {
    TestApp::Options options;
    options.args = L"--test-open-settings";
    options.env = {{L"WORDLOCK_TEST_COUNTDOWN_SECONDS", L"3"}};
    options.countdownTickMs = 100;
    options.prefix = L"PomodoroLockSettingsCountdown";
    return options;
}

}  // namespace

TEST_CASE("Settings window stops the countdown and closing it resumes the countdown",
          "[settings][countdown][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp app(SettingsCountdownOptions());
    const HWND settingWindow = WaitForWindow(app.settingClass, true, 5000);
    REQUIRE(settingWindow != nullptr);
    const HWND countdown = WaitForWindow(app.countdownClass, true, 5000);
    REQUIRE(countdown != nullptr);
    const HWND mainWindow = app.Main();
    REQUIRE(mainWindow != nullptr);
    REQUIRE(!IsWindowVisible(mainWindow));

    // Longer than the whole countdown: if the timer were still running while
    // settings were open, the main window would have replaced it by now.
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    REQUIRE(IsWindow(countdown));
    REQUIRE(IsWindowVisible(countdown));
    REQUIRE(!IsWindowVisible(mainWindow));

    PostMessageW(settingWindow, WM_CLOSE, 0, 0);
    REQUIRE(WaitForHiddenWindow(settingWindow, 3000));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline && !IsWindowVisible(mainWindow)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    REQUIRE(IsWindowVisible(mainWindow));
    REQUIRE(!IsWindowVisible(countdown));

    app.Quit();
}

TEST_CASE("Tray pause keeps the countdown stopped after the settings window closes",
          "[settings][countdown][pause][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp app(SettingsCountdownOptions());
    const HWND settingWindow = WaitForWindow(app.settingClass, true, 5000);
    REQUIRE(settingWindow != nullptr);
    const HWND countdown = WaitForWindow(app.countdownClass, true, 5000);
    REQUIRE(countdown != nullptr);
    const HWND mainWindow = app.Main();
    REQUIRE(mainWindow != nullptr);

    // This is the same pause path used by the tray menu's "Pause Countdown".
    PostMessageW(countdown, wordlock::kTestPauseCountdownMessage, 0, 0);
    REQUIRE(app.WaitForMessage("countdown:pause", 3000));

    PostMessageW(settingWindow, WM_CLOSE, 0, 0);
    REQUIRE(WaitForHiddenWindow(settingWindow, 3000));

    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    REQUIRE(IsWindowVisible(countdown));
    REQUIRE(!IsWindowVisible(mainWindow));

    // Resuming explicitly from the tray must still let the countdown finish.
    PostMessageW(countdown, wordlock::kTestResumeCountdownMessage, 0, 0);
    REQUIRE(app.WaitForMessage("countdown:resume", 3000));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline && !IsWindowVisible(mainWindow)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    REQUIRE(IsWindowVisible(mainWindow));

    app.Quit();
}
