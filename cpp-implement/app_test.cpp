// app_test.cpp
//
// Headless feature-test runner. Creates a real KotobaCppApp (windows exist,
// hidden, no message pump), then drives it through a script of `app` and
// ScriptRunner commands. `app tick` simulates the countdown plus any pending
// one-shot timers, so no real timers, no sleeping, no launched process. Event
// handlers are invoked directly (as ScriptRunner already does), which is why
// no message loop is needed.
//
// Build:  cmake --build build --config Release   (target: app_test)
// Usage:  build\app_test.exe --script tests\features\stop_start\steps.txt
//
// Script format (one command per line, # starts a comment):
//   app init                       create windows (hidden), start countdown
//   app word tests/word.json       fixed word (no random pick)
//   app tick [n]                   simulate n countdown ticks + one-shot timers
//   app assert <key> == <value>    key in {stopped,countdown,inputDone,word,
//                                   inputWin,infoWin,timers,freezeCalls,
//                                   resumeCalls} or `visible <window>`
//   app state                      print stateJson() (for debugging)
//   window <name>                  bind to countdown/tray/input/info/settings
//   click #id / type #id text / assert ...
//                                   (standard ScriptRunner commands)
//
// A feature test ships as tests/features/<name>/steps.txt and must fail before
// the change and pass after. run_tests.bat loops over features\*.

#include "kotoba_app.h"
#include "app/script.h"
#include "core/utf.h"
#include "utils/word_db.h"

#include <fstream>
#include <iostream>

using namespace ui;

struct AppRunner {
    KotobaCppApp app;
    ScriptRunner runner;             // bound to a window via `window`
    int appFailures = 0;

    void fail(const std::wstring& msg) {
        ++appFailures;
        std::wcout << L"FAIL: " << msg << L"\n";
    }

    void bindWindow(const std::wstring& name) {
        WindowHost* h = app.window(name);
        if (!h) { fail(L"window not created: " + name); return; }
        RECT rc; GetClientRect(h->hwnd(), &rc);
        runner.bind(h->document(), rc.right, rc.bottom, h->dpiScale());
    }

    void assertState(const std::vector<std::wstring>& t) {
        // `app assert <key> == <value>`; key may be `visible <window>`.
        if (t.size() < 4) { fail(L"app assert: need <key> == <value>"); return; }
        const std::wstring& key = t[2];
        const std::wstring& expected = t.back();
        std::wstring actual;
        if (key == L"visible") {
            if (t.size() < 5) { fail(L"app assert visible: need <window>"); return; }
            actual = app.isVisible(t[3]) ? L"true" : L"false";
        }
        else {
            auto state = app.stateJson();
            std::string k = wideToUtf8(key);
            if (!state.contains(k)) { fail(L"app assert: unknown key " + key); return; }
            auto& v = state[k];
            // String state (word) compares unquoted; everything else dumps
            // (bools -> true/false, ints -> digits).
            actual = v.is_string() ? utf8ToWide(v.get<std::string>())
                                    : utf8ToWide(v.dump());
        }
        if (actual != expected)
            fail(L"assert " + key + L" expected " + expected + L" got " + actual);
    }

    void appCmd(const std::vector<std::wstring>& t) {
        if (t[1] == L"init") {
            if (!app.init()) fail(L"app init failed");
        }
        else if (t[1] == L"tick") {
            app.tick(t.size() > 2 ? _wtoi(t[2].c_str()) : 1);
        }
        else if (t[1] == L"word") {
            if (t.size() < 3) { fail(L"app word: missing path"); return; }
            std::ifstream f(std::string(wideToUtf8(t[2])), std::ios::binary);
            if (!f) { fail(L"app word: cannot open " + t[2]); return; }
            app.setWord(std::string((std::istreambuf_iterator<char>(f)), {}));
        }
        else if (t[1] == L"assert") assertState(t);
        else if (t[1] == L"state") std::wcout << utf8ToWide(app.stateJson().dump(2)) << L"\n";
        else fail(L"unknown app command " + t[1]);
    }

    void runLine(const std::wstring& line) {
        auto t = ScriptRunner::tokens(line);
        if (t.empty() || t[0][0] == L'#') return;
        if (t[0] == L"app") { appCmd(t); return; }
        if (t[0] == L"window") {
            if (t.size() < 2) { fail(L"window: need a name"); return; }
            bindWindow(t[1]);
            return;
        }
        runner.run(line);
    }

    int failuresTotal() const { return appFailures + runner.failures(); }
};

int wmain(int argc, wchar_t** argv) {
    std::wstring script;
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--script") script = argv[++i];
    }
    if (script.empty()) {
        std::wcout << L"usage: app_test.exe --script <steps.txt>\n";
        return 2;
    }

    AppRunner r;
    std::ifstream in(wideToUtf8(script));
    if (!in) {
        std::wcout << L"cannot open " << script << L"\n";
        return 2;
    }
    std::string line;
    while (std::getline(in, line))
        r.runLine(utf8ToWide(line));

    std::wcout << r.runner.summary();
    int total = r.failuresTotal();
    std::wcout << (total ? L"FAILED " : L"OK ") << total << L" failure(s) total\n";
    return total ? 1 : 0;
}
