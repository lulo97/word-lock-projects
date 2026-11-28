#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

TEST_CASE("Clicking Ignore words button opens IgnoreWordsWindows",
          "[settings][ignore-words][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp app({L"--test-open-settings", {},
                 "document.getElementById('openIgnoreWords').click();", true});

    REQUIRE(WaitForNonEmptyFile(app.dumpFile, 5000));
    const HWND ignoreWindow = WaitForWindow(app.ignoreClass, true, 3000);
    INFO("Host messages: " << ReadFile(app.messageLog));
    REQUIRE(ignoreWindow != nullptr);
    REQUIRE(IsWindowVisible(ignoreWindow));

    REQUIRE(app.Main() != nullptr);
    app.Quit();
}
