#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

void CheckXClose(bool autoThemeEnabled) {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp::Options options;
    options.env = {{L"WORDLOCK_TEST_COUNTDOWN_SECONDS", L"1"},
                   {L"WORDLOCK_TEST_AUTO_THEME", autoThemeEnabled ? L"1" : L"0"}};
    options.countdownTickMs = 100;
    options.prefix = autoThemeEnabled ? L"PomodoroLockMainXOn" : L"PomodoroLockMainXOff";
    TestApp app(options);

    const HWND mainWindow = WaitForWindow(app.mainClass, true, 5000);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(app.WaitForMessage("word:getRandom", 3000));

    REQUIRE(PostMessageW(mainWindow, WM_CLOSE, 0, 0));

    const bool mainHidden = WaitForHiddenWindow(mainWindow, 1500);
    const bool countdownVisible = app.WaitVisible(app.countdownClass, 1500);
    const bool themeSwitched = app.WaitForMessage("countdown:theme:brutal", 1500);
    const bool mainReturned = app.WaitVisible(app.mainClass, 3000);
    const bool nextTask = app.WaitForMessage("mcq:getRandom", 3000);

    INFO("Auto theme enabled: " << autoThemeEnabled);
    INFO("Host messages: " << ReadFile(app.messageLog));
    INFO("Main hidden after X: " << mainHidden);
    INFO("Countdown visible after X: " << countdownVisible);

    REQUIRE(mainHidden);
    REQUIRE(countdownVisible);
    REQUIRE(mainReturned);
    REQUIRE(nextTask);
    REQUIRE(themeSwitched == autoThemeEnabled);
    app.Quit();
}

}  // namespace

TEST_CASE("Main X advances the next task with auto theme on",
          "[main][skip][theme][runtime]") {
    CheckXClose(true);
}

TEST_CASE("Main X advances the next task with auto theme off",
          "[main][skip][runtime]") {
    CheckXClose(false);
}
