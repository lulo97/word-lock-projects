#include <catch2/catch_test_macros.hpp>

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr UINT kTestQuitMessage = WM_APP + 2;

std::filesystem::path CurrentExecutablePath() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(std::wstring(buffer, length));
}

std::filesystem::path TempRoot() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, buffer);
    return std::filesystem::path(std::wstring(buffer, length));
}

std::filesystem::path AppExecutablePath() {
    const std::filesystem::path testExe = CurrentExecutablePath();
    const std::filesystem::path configDir = testExe.parent_path();
    const std::filesystem::path buildDir = configDir.parent_path().parent_path();
    return buildDir / configDir.filename() / L"PomodoroLockCpp.exe";
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

bool WaitForNonEmptyFile(const std::filesystem::path& path, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        if (std::filesystem::exists(path, error) &&
            std::filesystem::file_size(path, error) > 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

bool WaitForAllMessages(const std::filesystem::path& path,
                        const std::vector<std::string>& needles,
                        DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const std::string contents = ReadFile(path);
        bool allFound = true;
        for (const std::string& needle : needles) {
            if (contents.find(needle) == std::string::npos) {
                allFound = false;
                break;
            }
        }
        if (allFound) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

HWND WaitForVisibleWindow(const std::wstring& className, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        HWND window = FindWindowW(className.c_str(), nullptr);
        if (window != nullptr && IsWindowVisible(window)) {
            return window;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return nullptr;
}

struct ProcessGuard {
    PROCESS_INFORMATION process{};

    ProcessGuard() = default;
    ProcessGuard(const ProcessGuard&) = delete;
    ProcessGuard& operator=(const ProcessGuard&) = delete;

    ~ProcessGuard() {
        if (process.hProcess != nullptr) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                TerminateProcess(process.hProcess, 1);
            }
            CloseHandle(process.hProcess);
        }
        if (process.hThread != nullptr) {
            CloseHandle(process.hThread);
        }
    }

    void Dismiss() { process = {}; }
};

void RunProgressScenario(const std::filesystem::path& app,
                         const std::wstring& languageTag,
                         const std::wstring& launchArg,
                         const std::string& script,
                         const std::vector<std::string>& requiredMessages) {
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockProgress_" + languageTag + L"_" +
                      std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(stamp));
    const std::filesystem::path dumpFile = testDir / L"word_dom.json";
    const std::filesystem::path commandFile = testDir / L"command.js";
    const std::filesystem::path messageLog = testDir / L"messages.log";
    std::filesystem::create_directories(testDir);

    {
        std::ofstream command(commandFile, std::ios::binary | std::ios::trunc);
        command << script;
    }

    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_HOME", testDir.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_DOM_DUMP_FILE", dumpFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COMMAND_FILE", commandFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MESSAGE_LOG_FILE", messageLog.c_str()));

    std::wstring commandLine = L"\"" + app.wstring() + L"\" " + launchArg;
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ProcessGuard guard;
    REQUIRE(CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &startup, &guard.process));

    const std::wstring mainClass =
        L"MainWindowTest" + std::to_wstring(guard.process.dwProcessId);
    REQUIRE(WaitForVisibleWindow(mainClass, 4000) != nullptr);
    const bool dumpReady = WaitForNonEmptyFile(dumpFile, 5000);
    INFO("DOM dump: " << ReadFile(dumpFile));
    INFO("Host messages before wait: " << ReadFile(messageLog));
    REQUIRE(dumpReady);

    const bool messagesReady = WaitForAllMessages(messageLog, requiredMessages, 4000);
    INFO("Host messages after wait: " << ReadFile(messageLog));
    REQUIRE(messagesReady);

    HWND mainWindow = FindWindowW(mainClass.c_str(), nullptr);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(PostMessageW(mainWindow, kTestQuitMessage, 0, 0));
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
}

}  // namespace

TEST_CASE("Correct answers raise show count and random picks favor least shown words",
          "[progress][database][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const std::string englishScript = R"JS((async function () {
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

    const std::string japaneseScript = R"JS((async function () {
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

    RunProgressScenario(app, L"english", L"--test-open-english", englishScript,
                        {"en:progress-memory", "en:progress-empty", "en:progress-count",
                         "en:progress-phase2", "en:progress-persisted", "en:progress-least",
                         "en:progress-random"});

    RunProgressScenario(app, L"japanese", L"--test-open-japanese", japaneseScript,
                        {"jp:progress-memory", "jp:progress-empty", "jp:progress-count",
                         "jp:progress-phase2", "jp:progress-persisted", "jp:progress-least",
                         "jp:progress-random"});
}
