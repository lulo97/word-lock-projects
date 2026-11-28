#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

constexpr char kEnglishScript[] = R"JS((async function () {
  'use strict';
  function sleep(ms) { return new Promise(function (resolve) { setTimeout(resolve, ms); }); }
  function signal(type) { window.app.send(type); }
  async function waitForWord() {
    for (var i = 0; i < 100; ++i) {
      var el = document.getElementById('word');
      if (el && el.textContent && el.dataset.id) return el;
      await sleep(20);
    }
    return null;
  }

  var wordEl = await waitForWord();
  if (!wordEl) { signal('en:progress-no-word'); return; }
  var id = Number(wordEl.dataset.id);

  if (window.location.search.indexOf('phase=2') < 0) {
    var info = await window.app.call('db:info');
    if (info && info.inMemory) signal('en:progress-memory');
    var before = await window.app.call('progress:get');
    if (before && before.length === 0) signal('en:progress-empty');

    var answer = document.getElementById('answer');
    answer.value = wordEl.textContent;
    answer.dispatchEvent(new Event('input', { bubbles: true }));

    var feedback = document.getElementById('feedback');
    for (var i = 0; i < 100 && feedback.textContent !== 'Correct'; ++i) await sleep(10);
    var after = await window.app.call('progress:get');
    for (var j = 0; j < after.length; ++j) {
      if (after[j].language === 'english' && after[j].wordId === id && after[j].showCount === 1) {
        signal('en:progress-count');
      }
    }
    window.location.replace('english.html?phase=2&corrected=' + encodeURIComponent(id));
    return;
  }

  signal('en:progress-phase2');
  var correctedId = Number(new URLSearchParams(window.location.search).get('corrected'));
  var persisted = await window.app.call('progress:get');
  var correctedRow = null;
  for (var k = 0; k < persisted.length; ++k) {
    if (persisted[k].language === 'english' && persisted[k].wordId === correctedId) {
      correctedRow = persisted[k];
    }
  }
  if (correctedRow && correctedRow.showCount === 1) signal('en:progress-persisted');

  var lowest = {};
  var sawCorrected = false;
  var picks = 0;
  for (var n = 0; n < 60; ++n) {
    var picked = await window.app.call('word:getRandom');
    if (!picked) continue;
    ++picks;
    if (picked.id === correctedId) sawCorrected = true;
    else lowest[picked.id] = true;
  }
  if (picks >= 50 && !sawCorrected) signal('en:progress-least');
  if (Object.keys(lowest).length >= 3) signal('en:progress-random');
})();)JS";

constexpr char kJapaneseScript[] = R"JS((async function () {
  'use strict';
  function sleep(ms) { return new Promise(function (resolve) { setTimeout(resolve, ms); }); }
  function signal(type) { window.app.send(type); }
  async function waitForWord() {
    for (var i = 0; i < 100; ++i) {
      var el = document.getElementById('word');
      if (el && el.textContent && el.dataset.id) return el;
      await sleep(20);
    }
    return null;
  }

  var wordEl = await waitForWord();
  if (!wordEl) { signal('jp:progress-no-word'); return; }
  var id = Number(wordEl.dataset.id);

  if (window.location.search.indexOf('phase=2') < 0) {
    var info = await window.app.call('db:info');
    if (info && info.inMemory) signal('jp:progress-memory');
    var before = await window.app.call('progress:get');
    if (before && before.length === 0) signal('jp:progress-empty');

    var answer = document.getElementById('answer');
    answer.value = document.getElementById('romaji').textContent;
    answer.dispatchEvent(new Event('input', { bubbles: true }));

    var feedback = document.getElementById('feedback');
    for (var i = 0; i < 100 && feedback.textContent !== 'Correct'; ++i) await sleep(10);
    var after = await window.app.call('progress:get');
    for (var j = 0; j < after.length; ++j) {
      if (after[j].language === 'japanese' && after[j].wordId === id && after[j].showCount === 1) {
        signal('jp:progress-count');
      }
    }
    window.location.replace('japanese.html?phase=2&corrected=' + encodeURIComponent(id));
    return;
  }

  signal('jp:progress-phase2');
  var correctedId = Number(new URLSearchParams(window.location.search).get('corrected'));
  var persisted = await window.app.call('progress:get');
  var correctedRow = null;
  for (var k = 0; k < persisted.length; ++k) {
    if (persisted[k].language === 'japanese' && persisted[k].wordId === correctedId) {
      correctedRow = persisted[k];
    }
  }
  if (correctedRow && correctedRow.showCount === 1) signal('jp:progress-persisted');

  var lowest = {};
  var sawCorrected = false;
  var picks = 0;
  for (var n = 0; n < 60; ++n) {
    var picked = await window.app.call('japanese:getRandom');
    if (!picked) continue;
    ++picks;
    if (picked.id === correctedId) sawCorrected = true;
    else lowest[picked.id] = true;
  }
  if (picks >= 50 && !sawCorrected) signal('jp:progress-least');
  if (Object.keys(lowest).length >= 3) signal('jp:progress-random');
})();)JS";

void RunProgressScenario(const std::wstring& languageTag, const std::wstring& launchArg,
                         const std::string& script,
                         const std::vector<std::string>& requiredMessages) {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp::Options options;
    options.args = launchArg;
    options.commandScript = script;
    options.wantDomDump = true;
    options.prefix = L"PomodoroLockProgress_" + languageTag;
    TestApp app(options);

    REQUIRE(WaitForWindow(app.mainClass, true, 4000) != nullptr);
    REQUIRE(WaitForNonEmptyFile(app.dumpFile, 5000));
    INFO("DOM dump: " << ReadFile(app.dumpFile));
    REQUIRE(WaitForAllMessages(app.messageLog, requiredMessages, 4000));
    INFO("Host messages after wait: " << ReadFile(app.messageLog));
    app.Quit();
}

}  // namespace

TEST_CASE("English progress counts correct answers and favors least shown words",
          "[progress][database][english][runtime]") {
    RunProgressScenario(L"english", L"--test-open-english", kEnglishScript,
                        {"en:progress-memory", "en:progress-empty", "en:progress-count",
                         "en:progress-phase2", "en:progress-persisted", "en:progress-least",
                         "en:progress-random"});
}

TEST_CASE("Japanese progress counts correct answers and favors least shown words",
          "[progress][database][japanese][runtime]") {
    RunProgressScenario(L"japanese", L"--test-open-japanese", kJapaneseScript,
                        {"jp:progress-memory", "jp:progress-empty", "jp:progress-count",
                         "jp:progress-phase2", "jp:progress-persisted", "jp:progress-least",
                         "jp:progress-random"});
}
