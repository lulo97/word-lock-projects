#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

constexpr char kIgnoreWordsScript[] = R"JS((async function () {
  'use strict';
  function signal(type) { window.app.send(type); }
  function sleep(ms) { return new Promise(function (resolve) { setTimeout(resolve, ms); }); }
  async function wordElement() {
    for (var i = 0; i < 100; ++i) {
      var el = document.getElementById('word');
      if (el && el.textContent && el.dataset.id) return el;
      await sleep(20);
    }
    return null;
  }

  if (!await wordElement()) { signal('ignore:no-word'); return; }

  var saved = await window.app.call('ignoreWords:save', { english: 'alpha', japanese: '上' });
  if (saved && saved.ok) signal('ignore:save-ok');

  var current = await window.app.call('ignoreWords:get');
  if (current && current.english && current.english.length === 1 &&
      current.english[0] === 'alpha' && current.japanese && current.japanese[0] === '上') {
    signal('ignore:get-returns');
  }

  var invalid = await window.app.call('ignoreWords:save', { english: 'bad word' });
  if (invalid && invalid.ok === false) signal('ignore:invalid-rejected');

  var unchanged = await window.app.call('ignoreWords:get');
  if (unchanged && unchanged.english && unchanged.english[0] === 'alpha') {
    signal('ignore:state-unchanged');
  }

  var sawIgnored = false;
  var picks = 0;
  for (var n = 0; n < 40; ++n) {
    var picked = await window.app.call('word:getRandom');
    if (!picked) continue;
    ++picks;
    if (picked.word === 'alpha') sawIgnored = true;
  }
  if (picks >= 30 && !sawIgnored) signal('ignore:not-returned');
})();)JS";

}  // namespace

TEST_CASE("Ignore words save validates input and filters random words",
          "[ignore-words][runtime][webview]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp::Options options;
    options.args = L"--test-open-english";
    options.commandScript = kIgnoreWordsScript;
    options.wantDomDump = true;
    options.prefix = L"PomodoroLockIgnoreWordsSave";
    TestApp app(options);

    REQUIRE(WaitForWindow(app.mainClass, true, 4000) != nullptr);
    REQUIRE(WaitForNonEmptyFile(app.dumpFile, 5000));

    const std::vector<std::string> required{
        "ignore:save-ok", "ignore:get-returns", "ignore:invalid-rejected",
        "ignore:state-unchanged", "ignore:not-returned"};
    REQUIRE(WaitForAllMessages(app.messageLog, required, 6000));
    INFO("Host messages: " << ReadFile(app.messageLog));
    REQUIRE(ReadFile(app.messageLog).find("ignore:no-word") == std::string::npos);

    app.Quit();
}
