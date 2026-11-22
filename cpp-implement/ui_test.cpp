// ui_test.cpp
//
// Headless UI script runner: loads an HTML file, drives mouse/keyboard input
// deterministically, and answers with JSON dumps, PNG screenshots and a
// pass/fail exit code. No window is shown and Document::handleMessage is
// skipped (it reads GetKeyState/GET_X_LPARAM, which aren't deterministic);
// EventState is called directly instead.
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
#include "core/utf.h"
#include "controls/text_input.h"
#include "dom/tree_ops.h"
#include "utils/word_db.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace ui;

struct Runner {
    Document doc;
    int W = 1200, H = 800;
    float scale = 1.f;
    WordData word{};
    int failures = 0, lineNo = 0;

    void layout()  { doc.ensureLayout(W, H, scale); }
    void settle()  { doc.invalidateStyle(); layout(); }   // hover/focus => style re-resolve

    Element* need(const std::wstring& sel) {
        Element* e = doc.byId(sel.substr(sel[0] == L'#' ? 1 : 0));
        if (!e) fail(L"no element " + sel);
        return e;
    }
    void fail(const std::wstring& msg) {
        ++failures;
        std::wcout << L"FAIL line " << lineNo << L": " << msg << L"\n";
    }

    // Splits a line on spaces; "quoted text" tokens may contain spaces.
    static std::vector<std::wstring> tokens(const std::wstring& line) {
        std::vector<std::wstring> out;
        std::wstring cur;
        bool q = false, any = false;
        for (wchar_t c : line) {
            if (c == L'"') { q = !q; any = true; }
            else if (!q && iswspace(c)) { if (!cur.empty() || any) out.push_back(cur); cur.clear(); any = false; }
            else cur.push_back(c);
        }
        if (!cur.empty() || any) out.push_back(cur);
        return out;
    }

    void mouse(Element& el, bool click) {
        layout();
        RectF r = el.box.borderRect;
        float x = r.x + r.w / 2, y = r.y + r.h / 2;
        auto& ev = doc.events();
        ev.onMouseMove(*doc.root(), x, y, false);
        if (click) { ev.onMouseDown(*doc.root(), x, y); ev.onMouseUp(*doc.root(), x, y); }
        settle();
    }

    void key(const std::wstring& name) {
        auto& ev = doc.events();
        Element& root = *doc.root();
        if (name == L"Backspace") ev.onChar(root, L'\b');
        else {
            static const std::pair<const wchar_t*, UINT> map[] = {
                {L"Enter", VK_RETURN}, {L"Tab", VK_TAB}, {L"Escape", VK_ESCAPE},
                {L"Left", VK_LEFT}, {L"Right", VK_RIGHT}, {L"Up", VK_UP}, {L"Down", VK_DOWN},
                {L"Home", VK_HOME}, {L"End", VK_END}, {L"Delete", VK_DELETE}, {L"Space", VK_SPACE}};
            for (auto& m : map) if (name == m.first) { KeyEvent k{}; k.vk = m.second; ev.onKeyDown(root, k); }
        }
        settle();
    }

    // assert exists|visible #id   |   assert text|value #id == X   |   assert rect #id w>0
    void assertCmd(const std::vector<std::wstring>& t) {
        Element* e = need(t[2]);
        if (!e) return;
        const std::wstring& kind = t[1];
        layout();
        if (kind == L"exists") return;
        if (kind == L"visible") {
            if (e->style.display == Display::None || e->box.borderRect.w <= 0)
                fail(t[2] + L" not visible");
        } else if (kind == L"text" || kind == L"value") {
            std::wstring actual = kind == L"text" ? textContent(*e) : doc.getValue(e->id);
            if (actual != t[4]) fail(t[2] + L" " + kind + L" = '" + actual + L"' expected '" + t[4] + L"'");
        } else if (kind == L"rect") {
            const std::wstring& c = t[3];
            float v = c[0] == L'x' ? e->box.borderRect.x : c[0] == L'y' ? e->box.borderRect.y
                    : c[0] == L'w' ? e->box.borderRect.w : e->box.borderRect.h;
            size_t op = c.find_first_of(L"<>=");
            size_t vs = c.find_first_of(L"0123456789-", op);
            float rhs;
            parseFloat(c.substr(vs), rhs);
            std::wstring o = c.substr(op, vs - op);
            bool ok = o == L">" ? v > rhs : o == L">=" ? v >= rhs : o == L"<" ? v < rhs
                    : o == L"<=" ? v <= rhs : v == rhs;
            if (!ok) fail(t[2] + L" " + c + L" (actual " + std::to_wstring(v) + L")");
        }
    }

    void run(const std::wstring& line) {
        auto t = tokens(line);
        if (t.empty() || t[0][0] == L'#') return;
        const std::wstring& c = t[0];
        if (c == L"size")  { swscanf_s(t[1].c_str(), L"%dx%d", &W, &H); }
        else if (c == L"dpi") { parseFloat(t[1], scale); }
        else if (c == L"load") {
            std::ifstream f(std::string(wideToUtf8(t[1])), std::ios::binary);
            std::string bytes((std::istreambuf_iterator<char>(f)), {});
            std::wstring html = fill_template(utf8ToWide(bytes), word);
            if (!f || !doc.load(html)) fail(L"cannot load " + t[1]);
            layout();
        }
        else if (c == L"click") { if (auto* e = need(t[1])) mouse(*e, true); }
        else if (c == L"hover") { if (auto* e = need(t[1])) mouse(*e, false); }
        else if (c == L"focus") { if (auto* e = need(t[1])) { doc.events().setFocus(e); settle(); } }
        else if (c == L"type")  {
            if (auto* e = need(t[1])) {
                doc.events().setFocus(e);
                for (wchar_t ch : t[2]) doc.events().onChar(*doc.root(), ch);
                settle();
            }
        }
        else if (c == L"ime")   {
            if (auto* e = need(t[1]))
                if (auto* in = dynamic_cast<TextInput*>(e->control.get())) { in->onImeComposition(t[2], true); settle(); }
        }
        else if (c == L"key")   { key(t[1]); }
        else if (c == L"wheel") {
            layout();
            doc.events().onWheel(*doc.root(), (float)_wtof(t[1].c_str()), (float)_wtof(t[2].c_str()), _wtoi(t[3].c_str()));
            settle();
        }
        else if (c == L"dump")  { layout(); doc.dumpTree(t[1], 99); }
        else if (c == L"shot")  {
            doc.screenshot(t[1], W, H, scale, false);
            if (t.size() > 2 && t[2] == L"overlay")
                doc.screenshot(t[1] + L".overlay.png", W, H, scale, true);
        }
        else if (c == L"assert"){ assertCmd(t); }
        else fail(L"unknown command " + c);

        for (auto& p : doc.validate())
            fail(L"validate: " + p);
    }
};

int wmain(int argc, wchar_t** argv) {
    Runner r;
    std::wstring script;
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--script") script = argv[++i];
        else if (a == L"--word") {                       // deterministic template data
            std::ifstream f(wideToUtf8(argv[++i]), std::ios::binary);
            r.word = parse_word_json(std::string((std::istreambuf_iterator<char>(f)), {}));
        }
    }
    std::ifstream in(wideToUtf8(script));
    std::string line;
    while (std::getline(in, line)) { ++r.lineNo; r.run(utf8ToWide(line)); }
    std::wcout << (r.failures ? L"FAILED " : L"OK ") << r.failures << L" failure(s)\n";
    return r.failures ? 1 : 0;
}
