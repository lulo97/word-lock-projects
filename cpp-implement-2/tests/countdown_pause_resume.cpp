#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

TEST_CASE("Countdown pause freezes the timer and resume continues it",
          "[countdown][pause][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp::Options options;
    options.env = {{L"WORDLOCK_TEST_COUNTDOWN_SECONDS", L"3"}};
    options.countdownTickMs = 100;
    options.prefix = L"PomodoroLockCountdownPause";
    TestApp app(options);

    const HWND mainWindow = WaitForWindow(app.mainClass, false, 3000);
    REQUIRE(mainWindow != nullptr);
    HWND countdown = WaitForWindow(app.countdownClass, true, 3000);
    REQUIRE(countdown != nullptr);
    REQUIRE(!IsWindowVisible(mainWindow));

    // Pause: the timer must stop while the countdown window stays visible.
    PostMessageW(countdown, wordlock::kTestPauseCountdownMessage, 0, 0);
    REQUIRE(app.WaitForMessage("countdown:pause", 3000));

    // Longer than the whole 3-second countdown: if the timer were still
    // running, it would have finished and switched to the main window by
    // now.
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    REQUIRE(IsWindow(countdown));
    REQUIRE(IsWindowVisible(countdown));
    REQUIRE(!IsWindowVisible(mainWindow));

    // Resume: the remaining time keeps ticking down until the main window
    // replaces the countdown.
    PostMessageW(countdown, wordlock::kTestResumeCountdownMessage, 0, 0);
    REQUIRE(app.WaitForMessage("countdown:resume", 3000));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline && !IsWindowVisible(mainWindow)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    REQUIRE(IsWindowVisible(mainWindow));
    REQUIRE(!IsWindowVisible(countdown));

    app.Quit();
}
