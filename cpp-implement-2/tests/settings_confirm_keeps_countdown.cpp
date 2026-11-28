#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

TEST_CASE("Confirming settings keeps countdown visible", "[settings][countdown][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp app({L"--test-open-settings", {},
                 "document.getElementById('confirm').click();", true});

    REQUIRE(WaitForNonEmptyFile(app.dumpFile, 5000));
    REQUIRE(app.WaitForMessage("settings:confirm", 3000));
    INFO("Host messages: " << ReadFile(app.messageLog));

    // The countdown stays visible as soon as confirm hides the settings view.
    REQUIRE(app.WaitVisible(app.countdownClass, 1000));
    app.Quit();
}
