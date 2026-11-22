// fuzz.cpp
//
// Seeded fuzz + stress with validate(). Two loops:
//   1. Parser fuzz: random mutations of a real page must never crash, hang,
//      or produce an invalid layout.
//   2. Event storm: random input against a live document must never corrupt
//      it (dangling pointers, NaN rects, out-of-range scroll).
//
// The seed is printed so any failure is replayable:
//   build\fuzz.exe --seed 1 --iters 2000
//
// Build:  cmake --build build --config Release   (target: fuzz)
// ASan:   cmake -DUI_ENABLE_ASAN=ON -B build-asan -G "Visual Studio 18 2026" -A Win32

#include "app/document.h"
#include "core/utf.h"
#include "dom/tree_ops.h"
#include "../../utils/word_db.h"

#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace ui;

namespace {

std::string readFile(const std::wstring& path) {
    std::ifstream f(std::string(wideToUtf8(path)), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), {});
}

std::wstring readHtml(const std::wstring& path) {
    return utf8ToWide(readFile(path));
}

std::vector<std::wstring> collectIds(Document& d) {
    std::vector<std::wstring> ids;
    walk(*d.root(), [&](Element& el) {
        if (!el.id.empty()) ids.push_back(el.id);
        return true;
    });
    return ids;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    unsigned seed = 1;
    int iters = 2000;
    for (int i = 1; i + 1 < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--seed") seed = (unsigned)_wtoi(argv[++i]);
        else if (a == L"--iters") iters = _wtoi(argv[++i]);
    }
    std::wcout << L"seed=" << seed << L" iters=" << iters << L"\n";

    std::mt19937 rng(seed);
    auto rnd = [&](int n) { return (int)(rng() % (unsigned)n); };

    // Fill the page with a deterministic word so {{key}} placeholders are
    // resolved before fuzzing (raw templates would trip validate()).
    // Both reads must succeed: an empty file would fuzz a blank page and
    // report a misleading "OK".
    std::string wordJson = readFile(L"tests/word.json");
    if (wordJson.empty()) { std::wcout << L"cannot read tests/word.json (run from the project root)\n"; return 1; }
    std::wstring baseHtml = readHtml(L"htmls/input.html");
    if (baseHtml.empty()) { std::wcout << L"cannot read htmls/input.html (run from the project root)\n"; return 1; }
    std::wstring base = fill_template(baseHtml, parse_word_json(wordJson));

    // ---- 1. Parser fuzz: random mutations of a real page ----
    int rejected = 0;   // mutations the loader refused (tracked so a
                        // mostly-rejected corpus is visible)
    for (int i = 0; i < iters; ++i) {
        std::wstring s = base;
        for (int k = 0; k < 1 + rnd(10) && !s.empty(); ++k) {   // empty s => rnd(0) mod 0
            size_t pos = rnd((int)s.size());
            switch (rnd(3)) {
            case 0: s.erase(pos, rnd(20)); break;
            case 1: s.insert(pos, 1, L"<>/\"'&{}[]"[rnd(10)]); break;  // 10 chars, no NUL
            case 2: s.insert(pos, L"class=\"bg-[#zz] w-[1/0] -p-3\""); break;
            }
        }
        Document d;
        if (!d.load(s)) { ++rejected; continue; }
        d.ensureLayout(400 + rnd(800), 300 + rnd(600), 1.f + rnd(3) * 0.25f);
        auto problems = d.validate();
        if (!problems.empty()) {
            std::wcout << L"FUZZ-PROBLEM seed=" << seed << L" i=" << i;
            for (auto& p : problems) std::wcout << L"\n  " << p;
            std::wcout << L"\n";
            return 1;
        }
    }
    std::wcout << L"parser fuzz: " << iters << L" iters, " << rejected << L" rejected by loader\n";

    // ---- 2. Event storm: random events, then mutate the tree mid-flight ----
    Document d;
    if (!d.load(base)) { std::wcout << L"cannot load base page\n"; return 1; }
    d.ensureLayout(1200, 800, 1.f);
    auto ids = collectIds(d);
    for (int i = 0; i < iters * 2; ++i) {
        float x = rnd(1200), y = rnd(800);
        auto& ev = d.events();
        Element& root = *d.root();
        switch (rnd(8)) {
        case 0: ev.onMouseMove(root, x, y, rnd(2) != 0); break;
        case 1: ev.onMouseDown(root, x, y); break;
        case 2: ev.onMouseUp(root, x, y); break;
        case 3: ev.onChar(root, (wchar_t)(L'a' + rnd(26))); break;
        case 4: { KeyEvent k{}; k.vk = L"\x09\x0D\x25\x27\x08\x2E"[rnd(6)]; ev.onKeyDown(root, k); } break;
        case 5: ev.onWheel(root, x, y, rnd(2) ? 120 : -120); break;
        case 6: if (!ids.empty()) d.setText(ids[rnd((int)ids.size())], L"changed"); break;
        case 7:
            if (ids.empty()) break;
            d.clearChildren(ids[rnd((int)ids.size())]);
            ids = collectIds(d);
            break;
        }
        d.invalidateStyle();
        d.ensureLayout(1200, 800, 1.f);
        auto p = d.validate();
        if (!p.empty()) {
            std::wcout << L"EVENT-STORM seed=" << seed << L" i=" << i;
            for (auto& q : p) std::wcout << L"\n  " << q;
            std::wcout << L"\n";
            return 1;
        }
    }

    std::wcout << L"OK\n";
    return 0;
}
