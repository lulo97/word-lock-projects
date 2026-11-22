// ui_test.cpp
//
// Headless UI script runner: loads an HTML file, drives mouse/keyboard input
// deterministically, and answers with JSON dumps, PNG screenshots and a
// pass/fail exit code. No window is shown and Document::handleMessage is
// skipped (it reads GetKeyState/GET_X_LPARAM, which aren't deterministic);
// EventState is called directly instead.
//
// Thin wrapper over ui::ScriptRunner (ui/app/script.h), which is shared with
// the running app's debug command-file poller. Only `load` is local to the
// runner: it reads a file from disk and fills {{key}} templates with a
// deterministic --word file.
//
// Build:  cmake --build build --config Release   (target: ui_test)
// Usage:  build\ui_test.exe --word tests\taberu.json --script tests\input_basic.txt
//
// Script format (one command per line, # starts a comment):
//   size 1200x800
//   dpi 1.5
//   load htmls/input.html
//   shot out/00_initial.png
//   shot out/00_initial.png overlay
//   type #answer taberu
//   assert value #answer == taberu
//   click #some-button
//   hover #some-button
//   focus #some-button
//   key Enter
//   wheel 100 200 -120
//   dump out/after.json
//   assert exists #feedback
//   assert visible #feedback
//   assert text #feedback == Correct!
//   assert rect #answer w>0

#include "app/document.h"
#include "app/script.h"
#include "core/utf.h"
#include "utils/word_db.h"

#include <fstream>
#include <iostream>

using namespace ui;

struct Runner {
    Document doc;
    int W = 1200, H = 800;
    float scale = 1.f;
    WordData word{};
    ScriptRunner runner{ doc, W, H, scale };

    void loadCmd(const std::wstring& path) {
        std::wstring p = runner.resolvePath(path);
        std::ifstream f(std::string(wideToUtf8(p)), std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(f)), {});
        std::wstring html = fill_template(utf8ToWide(bytes), word);
        if (!f || !doc.load(html)) {
            runner.fail(L"cannot load " + path);
            return;
        }
        runner.layout();
    }
};

static std::wstring scriptDirOf(const std::wstring& script) {
    size_t slash = script.find_last_of(L"/\\");
    return slash == std::wstring::npos ? std::wstring(L".") : script.substr(0, slash);
}

int wmain(int argc, wchar_t** argv) {
    Runner r;
    std::wstring script;
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--script") {
            script = argv[++i];
            r.runner.setScriptDir(scriptDirOf(script));   // relative case paths
        }
        else if (a == L"--word") {                       // deterministic template data
            std::ifstream f(wideToUtf8(argv[++i]), std::ios::binary);
            r.word = parse_word_json(std::string((std::istreambuf_iterator<char>(f)), {}));
        }
    }
    std::ifstream in(wideToUtf8(script));
    std::string line;
    while (std::getline(in, line)) {
        std::wstring wline = utf8ToWide(line);
        auto t = ScriptRunner::tokens(wline);
        if (!t.empty() && t[0] == L"load") {
            r.loadCmd(t[1]);
            continue;
        }
        r.runner.run(wline);
    }
    std::wcout << r.runner.summary();
    return r.runner.failures() ? 1 : 0;
}
