#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

std::string NavigationScript(int stamp) {
    return "(function () { var key = 'wordlockThemeSwapConfirmed_" + std::to_string(stamp) + "';" +
           R"JS(
  var theme = document.documentElement && document.documentElement.dataset.theme
    ? document.documentElement.dataset.theme : '';
  window.app.send('theme:navigation:' + theme);

  var confirmed = false;
  try {
    confirmed = localStorage.getItem(key) === '1';
  } catch (error) { /* storage may be unavailable */ }

  if (confirmed) {
    window.app.send('theme:final:' + theme);
    return;
  }

  if (document.getElementById('answer')) {
    try { localStorage.setItem(key, '1'); } catch (error) {}
    window.app.call('settings:confirm', { theme: 'midnight' });
  }
})();)JS";
}

}  // namespace

TEST_CASE("Swapped theme survives the next task navigation",
          "[theme][settings][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    const int stamp = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
    TestApp::Options options;
    options.args = L"--test-open-english";
    options.commandScript = NavigationScript(stamp);
    options.wantDomDump = true;
    options.env = {{L"WORDLOCK_TEST_COUNTDOWN_SECONDS", L"2"}};
    options.prefix = L"PomodoroLockThemeSwap";
    TestApp app(options);

    const HWND mainWindow = WaitForWindow(app.mainClass, true, 3000);
    REQUIRE(mainWindow != nullptr);

    REQUIRE(app.WaitForMessage("settings:confirm", 4000));
    REQUIRE(app.WaitForMessage("theme:final:", 5000));

    const std::string messages = ReadFile(app.messageLog);
    INFO("Host messages: " << messages);
    REQUIRE(messages.find("theme:final:midnight") != std::string::npos);
    REQUIRE(messages.find("theme:final:paper") == std::string::npos);

    app.Quit();
}
