#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

TEST_CASE("Settings window renders Settings text at runtime",
          "[settings][runtime][webview]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp::Options options;
    options.args = L"--test-open-settings";
    options.wantDomDump = true;
    options.prefix = L"PomodoroLockCppTests";
    TestApp app(options);

    REQUIRE(app.WaitVisible(app.settingClass, 3000));
    REQUIRE(WaitForNonEmptyFile(app.dumpFile, 5000));
    const std::string domDump = ReadFile(app.dumpFile);
    INFO("Settings WebView DOM dump: " << domDump);
    REQUIRE(domDump.find("Settings") != std::string::npos);

    REQUIRE(app.Main() != nullptr);
    app.Quit();
}
