#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

constexpr char kOpenIgnoreScript[] =
    "document.getElementById('openIgnoreWords').click();";

constexpr char kClickSaveScript[] = R"JS((function () {
  'use strict';
  function signal(type) { window.app.send(type); }
  var words = document.getElementById('words');
  var save = document.getElementById('save');
  if (!words || !save) { signal('ignore:missing-save-controls'); return; }
  words.value = 'alpha';
  save.click();
  signal('ignore:save-clicked');
})();)JS";

}  // namespace

TEST_CASE("Saving ignore words closes only the ignore words window",
          "[settings][ignore-words][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp::Options options;
    options.args = L"--test-open-settings";
    options.commandScript = kOpenIgnoreScript;
    options.ignoreScript = kClickSaveScript;
    options.prefix = L"PomodoroLockIgnoreSaveKeepsSettings";
    TestApp app(options);

    const HWND settingWindow = WaitForWindow(app.settingClass, true, 5000);
    REQUIRE(settingWindow != nullptr);
    const HWND ignoreWindow = WaitForWindow(app.ignoreClass, true, 5000);
    REQUIRE(ignoreWindow != nullptr);
    REQUIRE(app.WaitForMessage("ignore:save-clicked", 5000));
    INFO("Host messages: " << ReadFile(app.messageLog));

    REQUIRE(WaitForHiddenWindow(ignoreWindow, 3000));

    // The settings window must stay open after the ignore words Save button.
    REQUIRE(IsWindow(settingWindow));
    REQUIRE(IsWindowVisible(settingWindow));

    app.Quit();
}
