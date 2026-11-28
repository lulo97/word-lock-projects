#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "app.h"
#include "support/app_harness.h"

using namespace wordlock;

namespace {

constexpr wchar_t kForceTtsMockEnv[] = L"WORDLOCK_TTS_TEST_FORCE_MOCK";

bool TtsServerAvailable() {
    HINTERNET session = WinHttpOpen(L"PomodoroLockCpp TTS probe/1.0",
                                    WINHTTP_ACCESS_TYPE_NO_PROXY,  // loopback: skip system proxy
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;

    // resolve, connect, send, receive (ms)
    WinHttpSetTimeouts(session, 300, 300, 1000, 1000);

    bool available = false;
    HINTERNET connect = WinHttpConnect(session, kTtsHost, kTtsPort, 0);
    if (connect) {
        HINTERNET request = WinHttpOpenRequest(connect, L"GET", L"/health",  // or L"/status"
                                               nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (request) {
            if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(request, nullptr)) {
                DWORD status = 0;
                DWORD size = sizeof(status);
                if (WinHttpQueryHeaders(request,
                                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                                        WINHTTP_NO_HEADER_INDEX)) {
                    available = (status == 200);
                }
            }
            WinHttpCloseHandle(request);
        }
        WinHttpCloseHandle(connect);
    }
    WinHttpCloseHandle(session);
    return available;
}

const std::string kSpeakScript = R"JS((async function () {
  'use strict';
  function sleep(ms) { return new Promise(function (resolve) { setTimeout(resolve, ms); }); }
  function signal(type) { window.app.send(type); }

  for (var i = 0; i < 100; ++i) {
    if (window.app && window.app.call) break;
    await sleep(20);
  }
  if (!window.app || !window.app.call) {
    signal('tts:no-app');
    return;
  }

  try {
    var request = { text: 'apple', lang: 'en' };
    var first = await window.app.call('tts:speak', request);
    var second = await window.app.call('tts:speak', request);
    await window.app.call('settings:set', { masterVolume: 2 });
    var third = await window.app.call('tts:speak', request);
    var ok = first && first.ok && second && second.ok && third && third.ok;
    var wavOk = ok && first.wav && second.wav && third.wav &&
                first.wav.indexOf('UklG') === 0 &&
                second.wav.indexOf('UklG') === 0 &&
                third.wav.indexOf('UklG') === 0;
    signal(wavOk ? 'tts:wav-ok' : 'tts:wav-bad');
    signal(ok && third.wav !== second.wav ? 'tts:gain-ok' : 'tts:gain-bad');
    signal('tts:script-done');
  } catch (error) {
    signal('tts:script-error');
  }
})();)JS";

}  // namespace

TEST_CASE("TTS speak caches raw audio and applies gain on every read",
          "[runtime][tts][cache]") {
    const bool useRealServer = !EnvFlagEnabled(kForceTtsMockEnv) && TtsServerAvailable();
    INFO("TTS server at 127.0.0.1:8080 " << (useRealServer ? "available" : "unavailable")
                                         << "; using "
                                         << (useRealServer ? "real TTS" : "mock TTS"));

    test_support::TestApp::Options options;
    options.args = L"--test-open-english";
    options.commandScript = kSpeakScript;
    options.env.emplace_back(kTtsMockEnv, useRealServer ? L"" : L"1");
    test_support::TestApp app(options);

    const bool scriptDone = app.WaitForMessage("tts:script-done", 25000);
    INFO("TTS host messages after wait: " << ReadFile(app.messageLog));
    REQUIRE(scriptDone);
    REQUIRE(app.WaitForMessage("tts:wav-ok", 1000));
    REQUIRE(app.WaitForMessage("tts:gain-ok", 1000));
    REQUIRE_FALSE(app.WaitForMessage("tts:wav-bad", 500));
    REQUIRE_FALSE(app.WaitForMessage("tts:script-error", 500));
    REQUIRE_FALSE(app.WaitForMessage("tts:no-app", 500));

    const std::string log = ReadFile(app.messageLog);
    INFO("TTS host messages: " << log);
    REQUIRE(CountOccurrences(log, "tts:cache-miss") >= 1);
    REQUIRE(CountOccurrences(log, "tts:cache-hit") >= 2);
    REQUIRE(CountOccurrences(log, "tts:network") == 1);
}
