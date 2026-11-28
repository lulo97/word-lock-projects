#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

constexpr char kAutofocusScript[] = R"JS((function () {
  var signaled = false;
  var timer = setInterval(function () {
    var active = document.activeElement;
    if (!signaled && active && active.id === 'answer') {
      signaled = true;
      window.app.send('input:autofocused');
    }
    var answer = document.getElementById('answer');
    if (answer && answer.value.toLowerCase() === 'abc') {
      clearInterval(timer);
      window.app.send('input:typed:abc');
    }
  }, 40);
  setTimeout(function () {
    if (!signaled) {
      clearInterval(timer);
      window.app.send('input:autofocus-failed');
    }
  }, 2500);
})();)JS";

void TypeAbc(HWND focusedWindow) {
    for (wchar_t character : std::wstring(L"abc")) {
        REQUIRE(PostMessageW(focusedWindow, WM_CHAR, static_cast<WPARAM>(character), 1));
    }
}

void RunAutofocusScenario(const std::string& languageTag, const std::wstring& launchArg) {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    const std::wstring languageWide(languageTag.begin(), languageTag.end());
    TestApp::Options options;
    options.args = launchArg;
    options.commandScript = kAutofocusScript;
    options.wantDomDump = true;
    options.prefix = L"PomodoroLockAutofocus_" + languageWide;
    TestApp app(options);

    const HWND mainWindow = WaitForWindow(app.mainClass, true, 4000);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(app.WaitForMessage("input:autofocused", 3000));
    INFO(languageTag << " host messages before typing: " << ReadFile(app.messageLog));

    DWORD appPid = 0;
    const DWORD appThread = GetWindowThreadProcessId(mainWindow, &appPid);
    GUITHREADINFO focusInfo{};
    focusInfo.cbSize = sizeof(focusInfo);
    const BOOL focusRead = GetGUIThreadInfo(appThread, &focusInfo);
    const bool focusIsChild = focusRead && IsChild(mainWindow, focusInfo.hwndFocus);
    INFO("foreground=" << GetForegroundWindow() << " main=" << mainWindow
                       << " focus=" << (focusRead ? focusInfo.hwndFocus : nullptr)
                       << " focusIsChild=" << focusIsChild);
    REQUIRE(focusIsChild);

    TypeAbc(focusInfo.hwndFocus);

    const bool typed = app.WaitForMessage("input:typed:abc", 2500);
    INFO(languageTag << " host messages after typing: " << ReadFile(app.messageLog));
    REQUIRE(typed);
    REQUIRE(ReadFile(app.messageLog).find("input:autofocus-failed") == std::string::npos);
    app.Quit();
}

}  // namespace

TEST_CASE("English word input is focused and ready for typing",
          "[input][focus][english][runtime]") {
    RunAutofocusScenario("english", L"--test-open-english");
}

TEST_CASE("Japanese word input is focused and ready for typing",
          "[input][focus][japanese][runtime]") {
    RunAutofocusScenario("japanese", L"--test-open-japanese");
}
