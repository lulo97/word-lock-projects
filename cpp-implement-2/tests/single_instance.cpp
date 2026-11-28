#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

TEST_CASE("Second instance exits without disturbing the first",
          "[single-instance][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::wstring mutexSuffix =
        L"test_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(stamp);

    TestApp::Options options;
    options.singleInstance = true;
    options.singleInstanceName = mutexSuffix;
    options.prefix = L"PomodoroLockSingleInstance";
    options.wantDomDump = true;
    options.args = L"--test-open-english";  // Keeps the main window visible.

    TestApp first(options);
    const HWND firstWindow = WaitForWindow(first.mainClass, true, 5000);
    REQUIRE(firstWindow != nullptr);
    REQUIRE(WaitForNonEmptyFile(first.dumpFile, 3000));

    DWORD firstExitCode = 0;
    REQUIRE(GetExitCodeProcess(first.process.hProcess, &firstExitCode));
    REQUIRE(firstExitCode == STILL_ACTIVE);

    TestApp second(options);
    DWORD secondExitCode = 1;
    REQUIRE(second.WaitForExit(3000, &secondExitCode));
    REQUIRE(secondExitCode == 0);
    INFO("Second instance messages: " << ReadFile(second.messageLog));
    REQUIRE(second.Main() == nullptr);

    // The winning instance is untouched and still responsive.
    REQUIRE(IsWindow(firstWindow));
    REQUIRE(first.Main() == firstWindow);
    REQUIRE(GetExitCodeProcess(first.process.hProcess, &firstExitCode));
    REQUIRE(firstExitCode == STILL_ACTIVE);
    REQUIRE(PostMessageW(firstWindow, wordlock::kTrayMessage, 0, WM_LBUTTONUP));
    REQUIRE(WaitForHiddenWindow(firstWindow, 1500));
    REQUIRE(first.WaitVisible(first.countdownClass, 1500));

    first.Quit();
}
