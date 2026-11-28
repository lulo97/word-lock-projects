#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

constexpr char kTraversalScript[] = R"JS((async function () {
  function signal(type) { window.app.send(type); }
  signal('app:resource-start');
  var variants = [
    'https://app/%2e%2e/secret.txt',
    'https://app/%2E%2E%2Fsecret.txt',
    'https://app/../secret.txt',
    'https://app/subdir/../../secret.txt'
  ];
  function withTimeout(promise) {
    return Promise.race([
      promise,
      new Promise(function (resolve) { setTimeout(function () { resolve('timeout'); }, 800); })
    ]);
  }
  for (var i = 0; i < variants.length; ++i) {
    var outcome;
    try {
      var response = await withTimeout(fetch(variants[i], { method: 'GET', cache: 'no-store' }));
      outcome = response === 'timeout' ? 'timeout' : 'status-' + response.status;
    } catch (error) {
      outcome = 'error';
    }
    signal('app:resource-' + i + '-' + outcome);
  }
  signal('app:resource-done');
})();)JS";

}  // namespace

TEST_CASE("App resource handler rejects path traversal with 403",
          "[uri][security][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp::Options options;
    options.args = L"--test-open-settings";
    options.commandScript = kTraversalScript;
    options.wantDomDump = true;
    options.prefix = L"PomodoroLockAppResource403";
    TestApp app(options);

    REQUIRE(app.WaitVisible(app.settingClass, 4000));
    REQUIRE(WaitForNonEmptyFile(app.dumpFile, 5000));
    REQUIRE(app.WaitForMessage("app:resource-done", 6000));
    INFO("Resource request signals: " << ReadFile(app.messageLog));
    REQUIRE(ReadFile(app.messageLog).find("-403") != std::string::npos);

    app.Quit();
}
