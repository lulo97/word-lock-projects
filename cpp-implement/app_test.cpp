// app_test.cpp
//
// Headless feature-test runner. Creates a real KotobaCppApp (windows exist,
// hidden, no message pump), then drives it through a script of `app` and
// ScriptRunner commands. `app tick` simulates the countdown plus any pending
// one-shot timers, so no real timers, no sleeping, no launched process. Event
// handlers are invoked directly (as ScriptRunner already does), which is why
// no message loop is needed.
//
// All commands come from the shared ScriptRunner registry: the built-in
// document commands (click/type/assert/...) plus the `app`/`window` commands
// registered by KotobaCppApp::registerCommands(). There is no app-local
// command code here; a feature test only writes steps.txt.
//
// Build:  cmake --build build --config Release   (target: app_test)
// Usage:  build\app_test.exe --script tests\features\stop_start\steps.txt
//
// A feature test ships as tests/features/<name>/steps.txt and must fail
// before the change and pass after. run.ps1 loops over features\* and
// reports FAIL <file>:<line> lines.

#include "kotoba_app.h"
#include "app/script.h"
#include "core/utf.h"

#include <fstream>
#include <iostream>

using namespace ui;

static std::wstring scriptDirOf(const std::wstring& script) {
    size_t slash = script.find_last_of(L"/\\");
    return slash == std::wstring::npos ? std::wstring(L".") : script.substr(0, slash);
}

int wmain(int argc, wchar_t** argv) {
    std::wstring script;
    for (int i = 1; i + 1 < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--script") script = argv[++i];
    }
    if (script.empty()) {
        std::wcout << L"usage: app_test.exe --script <steps.txt>\n";
        return 2;
    }

    KotobaCppApp app;
    // Config/progress live in the test's own folder: self-contained and
    // deterministic across runs and features.
    app.hooks.configPath = scriptDirOf(script) + L"\\config.json";
    app.hooks.progressPath = scriptDirOf(script) + L"\\progress.json";
    ScriptRunner runner;             // built-in commands
    app.registerCommands(runner);   // + `app` and `window`
    runner.setScriptDir(scriptDirOf(script));

    std::ifstream in(wideToUtf8(script));
    if (!in) {
        std::wcout << L"cannot open " << script << L"\n";
        return 2;
    }
    int line = 0;
    std::string raw;
    while (std::getline(in, raw)) {
        ++line;
        runner.setLocation(script, line);
        runner.run(utf8ToWide(raw));
    }
    std::wcout << runner.summary();
    return runner.failures() ? 1 : 0;
}
