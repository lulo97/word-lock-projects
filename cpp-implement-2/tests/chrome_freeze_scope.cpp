#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

TestApp::Options FreezeOptions() {
    TestApp::Options options;
    options.env = {{L"WORDLOCK_TEST_CHROME_FREEZE", L"1"}};
    options.freezeIntervalMs = 100;  // A 2 s timer becomes a 100 ms timer in tests.
    options.wantDomDump = true;
    return options;
}

enum class VisibleWindow { Countdown, Settings, Ignore };

void CheckNoFreezeWhileHidden(TestApp::Options options, VisibleWindow window) {
    TestApp app(options);
    REQUIRE(std::filesystem::exists(AppExecutablePath()));
    const std::wstring cls = window == VisibleWindow::Settings ? app.settingClass
                             : window == VisibleWindow::Ignore ? app.ignoreClass
                                                               : app.countdownClass;
    if (!options.commandScript.empty()) {
        REQUIRE(WaitForNonEmptyFile(app.dumpFile, 5000));
    }
    REQUIRE(app.WaitVisible(cls, 4000));
    REQUIRE(app.WaitForMessage("chrome:unfreeze", 2000));

    TruncateFile(app.messageLog);
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    INFO("Mock log while main is hidden: " << ReadFile(app.messageLog));
    REQUIRE(ReadFile(app.messageLog).find("chrome:freeze") == std::string::npos);

    // Exiting must explicitly unfreeze, even if nothing was ever frozen.
    app.Quit();
    REQUIRE(app.WaitForMessage("chrome:unfreeze", 2000));
}

}  // namespace

TEST_CASE("Chrome freeze follows main window visibility", "[chrome][freeze][mock][runtime]") {
    TestApp app(FreezeOptions());
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    const HWND mainWindow = WaitForWindow(app.mainClass, false, 3000);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(app.WaitForMessage("chrome:unfreeze", 2000));

    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    INFO("Mock log while main is hidden: " << ReadFile(app.messageLog));
    REQUIRE(ReadFile(app.messageLog).find("chrome:freeze") == std::string::npos);
    TruncateFile(app.messageLog);

    REQUIRE(PostMessageW(mainWindow, wordlock::kTrayMessage, 0, WM_LBUTTONUP));
    REQUIRE(app.WaitForMessage("chrome:freeze", 3000));
    REQUIRE(app.WaitVisible(app.mainClass, 2000));
    REQUIRE(app.WaitForMessage("chrome:freeze", 2500, 2));

    TruncateFile(app.messageLog);
    REQUIRE(PostMessageW(mainWindow, WM_CLOSE, 0, 0));
    REQUIRE(app.WaitForMessage("chrome:unfreeze", 2000));
    REQUIRE(!IsWindowVisible(mainWindow));

    // No re-freeze after the window is hidden, even across several timer ticks.
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    const std::string afterClose = ReadFile(app.messageLog);
    INFO("Mock log after main closes: " << afterClose);
    REQUIRE(afterClose.find("chrome:freeze") == std::string::npos);
    REQUIRE(CountOccurrences(afterClose, "chrome:unfreeze") == 1);

    app.Quit();
}

TEST_CASE("Settings window does not freeze Chrome", "[chrome][freeze][settings][runtime]") {
    TestApp::Options options = FreezeOptions();
    options.args = L"--test-open-settings";
    CheckNoFreezeWhileHidden(options, VisibleWindow::Settings);
}

TEST_CASE("Ignore words window does not freeze Chrome", "[chrome][freeze][ignore][runtime]") {
    TestApp::Options options = FreezeOptions();
    options.args = L"--test-open-settings";
    options.commandScript = "document.getElementById('openIgnoreWords').click();";
    CheckNoFreezeWhileHidden(options, VisibleWindow::Ignore);
}

TEST_CASE("Countdown does not freeze Chrome", "[chrome][freeze][countdown][runtime]") {
    CheckNoFreezeWhileHidden(FreezeOptions(), VisibleWindow::Countdown);
}
