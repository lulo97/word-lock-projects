#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

struct SharedHome {
    SharedHome() {
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
        path = TempRoot() / (L"PomodoroLockSettingsHome_" + std::to_wstring(GetCurrentProcessId()) +
                             L"_" + std::to_wstring(stamp));
        std::filesystem::create_directories(path);
    }
    ~SharedHome() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    SharedHome(const SharedHome&) = delete;
    SharedHome& operator=(const SharedHome&) = delete;

    std::filesystem::path path;
};

}  // namespace

TEST_CASE("Settings persist in settings.json across an app restart",
          "[settings][persistence][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));
    SharedHome home;

    TestApp::Options firstOptions;
    firstOptions.args = L"--test-open-settings";
    firstOptions.commandScript =
        "(async function () { await window.app.call('settings:confirm', "
        "{ theme: 'midnight', countdownSeconds: 42 }); "
        "window.app.send('settings:persist-saved'); })();";
    firstOptions.useTestData = false;
    firstOptions.testHome = home.path.wstring();
    firstOptions.prefix = L"PomodoroLockSettingsPersistFirst";

    {
        TestApp first(firstOptions);
        REQUIRE(first.WaitVisible(first.settingClass, 4000));
        REQUIRE(WaitForNonEmptyFile(first.dumpFile, 5000));
        REQUIRE(first.WaitForMessage("settings:persist-saved", 4000));
        INFO("First run messages: " << ReadFile(first.messageLog));
        first.Quit();
    }

    REQUIRE(std::filesystem::exists(home.path / L"settings.json"));

    TestApp::Options secondOptions = firstOptions;
    secondOptions.commandScript =
        "(async function () { var data = await window.app.call('settings:get'); "
        "if (data) window.app.send('settings:persist-' + data.theme + '-' + "
        "data.countdownSeconds); })();";
    secondOptions.prefix = L"PomodoroLockSettingsPersistSecond";

    TestApp second(secondOptions);
    REQUIRE(second.WaitVisible(second.settingClass, 4000));
    REQUIRE(WaitForNonEmptyFile(second.dumpFile, 5000));
    REQUIRE(second.WaitForMessage("settings:persist-midnight-42", 4000));
    INFO("Host messages after restart: " << ReadFile(second.messageLog));
    second.Quit();
}
