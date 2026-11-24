#include "app/script.h"

#include "controls/text_input.h"
#include "core/utf.h"
#include "dom/tree_ops.h"
#include "paint/text_draw.h"

#include "json.hpp"

#include <algorithm>
#include <cwchar>
#include <cmath>
#include <cstdlib>
#include <fstream>

namespace ui {

ScriptRunner::ScriptRunner() {
    registerBuiltinCommands();
}

ScriptRunner::ScriptRunner(Document& doc, int w, int h, float scale) : ScriptRunner() {
    bind(doc, w, h, scale);
}

void ScriptRunner::bind(Document& doc, int w, int h, float scale,
                        std::function<bool()> alive) {
    doc_ = &doc;
    alive_ = std::move(alive);
    W_ = w;
    H_ = h;
    scale_ = scale;
}

void ScriptRunner::unbind() {
    doc_ = nullptr;
    alive_ = nullptr;
    W_ = H_ = 0;
    scale_ = 1.f;
}

void ScriptRunner::setLocation(const std::wstring& file, int line) {
    file_ = file;
    lineNo_ = line;
}

void ScriptRunner::addCommand(const std::wstring& name, size_t minArgs,
                              const std::wstring& usage, CmdFn fn) {
    cmds_[name] = { minArgs, usage, std::move(fn) };
}

std::wstring ScriptRunner::summary() const {
    std::wstring s;
    for (const auto& m : messages_)
        s += m + L"\n";
    s += (failures_ ? L"FAILED " : L"OK ") + std::to_wstring(failures_) + L" failure(s)\n";
    return s;
}

void ScriptRunner::layout() {
    if (doc_)
        doc_->ensureLayout(W_, H_, scale_);
}

void ScriptRunner::settle() {
    if (doc_)
        doc_->invalidateStyle();   // hover/focus => style re-resolve
    layout();
}

Element* ScriptRunner::need(const std::wstring& sel) {
    if (!doc_) {
        fail(L"no window bound (need a `window` command first)");
        return nullptr;
    }
    Element* e = doc_->byId(sel.substr(sel[0] == L'#' ? 1 : 0));
    if (!e) fail(L"no element " + sel);
    return e;
}

void ScriptRunner::fail(const std::wstring& msg) {
    ++failures_;
    std::wstring loc = file_.empty() ? std::to_wstring(lineNo_)
                                      : file_ + L":" + std::to_wstring(lineNo_);
    messages_.push_back(L"FAIL " + loc + L": " + msg);
}

void ScriptRunner::note(const std::wstring& msg) {
    messages_.push_back(msg);
}

void ScriptRunner::resetReport() {
    failures_ = 0;
    messages_.clear();
}

// Splits a line on spaces; "quoted text" tokens may contain spaces.
std::vector<std::wstring> ScriptRunner::tokens(const std::wstring& line) {
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

void ScriptRunner::mouse(Element& el, bool click) {
    layout();
    RectF r = el.box.borderRect;
    float x = r.x + r.w / 2, y = r.y + r.h / 2;
    auto& ev = doc_->events();
    ev.onMouseMove(*doc_->root(), x, y, false);
    if (click) { ev.onMouseDown(*doc_->root(), x, y); ev.onMouseUp(*doc_->root(), x, y); }
    settle();
}

void ScriptRunner::keyCmd(const std::vector<std::wstring>& t) {
    if (!doc_) { fail(L"key: no window bound"); return; }
    const std::wstring& name = t[1];
    auto& ev = doc_->events();
    Element& root = *doc_->root();
    if (name == L"Ctrl+A") {
        KeyEvent k{}; k.vk = 'A'; k.ctrl = true;
        ev.onKeyDown(root, k);
        settle();
        return;
    }
    if (name == L"Backspace") ev.onChar(root, L'\b');
    else {
        static const std::pair<const wchar_t*, UINT> map[] = {
            {L"Enter", VK_RETURN}, {L"Tab", VK_TAB}, {L"Escape", VK_ESCAPE},
            {L"Left", VK_LEFT}, {L"Right", VK_RIGHT}, {L"Up", VK_UP}, {L"Down", VK_DOWN},
            {L"Home", VK_HOME}, {L"End", VK_END}, {L"Delete", VK_DELETE}, {L"Space", VK_SPACE}};
        bool found = false;
        for (auto& m : map)
            if (name == m.first) { KeyEvent k{}; k.vk = m.second; ev.onKeyDown(root, k); found = true; }
        if (!found) { fail(L"unknown key " + name); return; }
    }
    settle();
}

// assert [not] exists #id
// assert [not] visible #id          (hidden by own display/zero width OR
//                                     any ancestor's display:none)
// assert text|value #id == X        (the operator must be ==, exactly)
// assert caret #id == N
// assert rect #id x>0 | y>=0 | w<=0 | h==5   (== compares with 0.5 px slack)
void ScriptRunner::assertCmd(const std::vector<std::wstring>& t) {
    const std::wstring& k0 = t[1];
    bool negate = (k0 == L"not");
    const std::wstring& kind = negate ? t[2] : k0;
    // With `not` the #id argument shifts one token right: [assert, not, kind, #id, ...].
    const size_t a = negate ? 3 : 2;

    if (kind == L"exists") {
        if (t.size() <= a) { fail(L"assert: need #id"); return; }
        if (!doc_) { fail(L"assert exists: no window bound"); return; }
        Element* e = doc_->byId(t[a].substr(t[a][0] == L'#' ? 1 : 0));
        bool ok = !!e;
        if (negate) ok = !ok;
        if (!ok) fail(t[a] + L" " + (negate ? L"exists (expected absent)"
                                           : L"does not exist (expected present)"));
        return;
    }

    if (kind == L"visible") {
        if (t.size() <= a) { fail(L"assert: need #id"); return; }
        Element* e = need(t[a]);
        if (!e) return;
        layout();
        bool vis = true;
        for (Element* p = e; p; p = static_cast<Element*>(p->parent))
            if (p->style.display == Display::None) { vis = false; break; }
        if (vis && e->box.borderRect.w <= 0) vis = false;
        if (negate) vis = !vis;
        if (!vis) fail(t[a] + L" " + (negate ? L"visible (expected hidden)"
                                             : L"not visible (expected visible)"));
        return;
    }

    if (negate) {
        // Negation only makes sense for existence/visibility; equality,
        // caret and rect checks are not invertible.
        fail(L"assert not " + kind + L": 'not' is only supported for exists/visible");
        return;
    }

    if (kind == L"text" || kind == L"value") {
        if (t.size() < 5) { fail(L"assert " + kind + L": need #id == value"); return; }
        Element* e = need(t[2]);
        if (!e) return;
        if (t[3] != L"==") { fail(L"assert " + kind + L": operator must be ==, got " + t[3]); return; }
        layout();
        std::wstring actual = kind == L"text" ? textContent(*e) : doc_->getValue(e->id);
        if (kind == L"text") {
            // Trim the HTML indentation whitespace: an element's visible
            // text is what the user sees, and reformatting the HTML
            // (prettier) must not break exact-text assertions.
            size_t b = actual.find_first_not_of(L" \t\r\n");
            size_t e2 = actual.find_last_not_of(L" \t\r\n");
            actual = (b == std::wstring::npos) ? L"" : actual.substr(b, e2 - b + 1);
        }
        if (actual != t[4]) fail(t[2] + L" " + kind + L" = '" + actual + L"' expected '" + t[4] + L"'");
        return;
    }

    if (kind == L"caret") {
        if (t.size() < 5) { fail(L"assert caret: need #id == N"); return; }
        Element* e = need(t[2]);
        if (!e) return;
        if (t[3] != L"==") { fail(L"assert caret: operator must be ==, got " + t[3]); return; }
        auto* in = dynamic_cast<TextInput*>(e->control.get());
        if (!in || in->caret != _wtoi(t[4].c_str()))
            fail(t[2] + L" caret=" + (in ? std::to_wstring(in->caret) : std::wstring(L"n/a")) +
                 L" expected " + t[4]);
        return;
    }

    if (kind == L"rect") {
        if (t.size() < 4) { fail(L"assert rect: need #id <comp>"); return; }
        Element* e = need(t[2]);
        if (!e) return;
        layout();
        const std::wstring& c = t[3];
        float v = c[0] == L'x' ? e->box.borderRect.x : c[0] == L'y' ? e->box.borderRect.y
                : c[0] == L'w' ? e->box.borderRect.w : e->box.borderRect.h;
        size_t op = c.find_first_of(L"<>=");
        size_t vs = c.find_first_of(L"0123456789-", op);
        float rhs = 0.f;
        if (op == std::wstring::npos || vs == std::wstring::npos || !parseFloat(c.substr(vs), rhs)) {
            fail(L"assert rect: bad comparison " + c);
            return;
        }
        std::wstring o = c.substr(op, vs - op);
        // == compares with 0.5 px slack: exact float comparison is flaky at
        // DPI 1.25/1.5.
        bool ok = o == L">" ? v > rhs : o == L">=" ? v >= rhs : o == L"<" ? v < rhs
                : o == L"<=" ? v <= rhs : std::fabs(v - rhs) <= 0.5f;
        if (!ok) fail(t[2] + L" " + c + L" (actual " + std::to_wstring(v) + L")");
        return;
    }

    fail(L"unknown assert kind " + kind + (negate ? L" (after 'not')" : L""));
}

void ScriptRunner::includeCmd(const std::vector<std::wstring>& t) {
    std::wstring p = resolvePath(t[1]);
    std::ifstream f(std::string(wideToUtf8(p)));
    if (!f) { fail(L"include: cannot open " + p); return; }
    if (++includeDepth_ > 8) {
        fail(L"include: nesting too deep (cycle?)");
        --includeDepth_;
        return;
    }
    std::string line;
    int n = 0;
    while (std::getline(f, line)) {
        ++n;
        setLocation(p, n);
        run(utf8ToWide(line));
    }
    --includeDepth_;
}

void ScriptRunner::registerBuiltinCommands() {
    addCommand(L"size", 1, L"size WxH",
        [this](const auto& t, const auto&) { swscanf_s(t[1].c_str(), L"%dx%d", &W_, &H_); });
    addCommand(L"dpi", 1, L"dpi <scale>",
        [this](const auto& t, const auto&) { parseFloat(t[1], scale_); });
    addCommand(L"click", 1, L"click #id",
        [this](const auto& t, const auto&) { if (auto* e = need(t[1])) mouse(*e, true); });
    addCommand(L"hover", 1, L"hover #id",
        [this](const auto& t, const auto&) { if (auto* e = need(t[1])) mouse(*e, false); });
    addCommand(L"focus", 1, L"focus #id",
        [this](const auto& t, const auto&) {
            if (auto* e = need(t[1])) { doc_->events().setFocus(e); settle(); }
        });
    addCommand(L"type", 2, L"type #id text",
        [this](const auto& t, const auto&) {
            if (auto* e = need(t[1])) {
                doc_->events().setFocus(e);
                for (wchar_t ch : t[2]) doc_->events().onChar(*doc_->root(), ch);
                settle();
            }
        });
    addCommand(L"ime", 2, L"ime #id text",
        [this](const auto& t, const auto&) {
            if (auto* e = need(t[1]))
                if (auto* in = dynamic_cast<TextInput*>(e->control.get())) { in->onImeComposition(t[2], true); settle(); }
        });
    addCommand(L"key", 1, L"key <Enter|Tab|Escape|...|Backspace|Ctrl+A>",
        [this](const auto& t, const auto&) { keyCmd(t); });
    addCommand(L"wheel", 3, L"wheel x y delta",
        [this](const auto& t, const auto&) {
            if (!doc_) { fail(L"wheel: no window bound"); return; }
            layout();
            doc_->events().onWheel(*doc_->root(), (float)_wtof(t[1].c_str()),
                                   (float)_wtof(t[2].c_str()), _wtoi(t[3].c_str()));
            settle();
        });
    addCommand(L"dump", 1, L"dump out.json",
        [this](const auto& t, const auto&) {
            if (!doc_) { fail(L"dump: no window bound"); return; }
            layout();
            doc_->dumpTree(t[1], 99);
        });
    addCommand(L"shot", 1, L"shot out.png [overlay]",
        [this](const auto& t, const auto&) {
            if (!doc_) { fail(L"shot: no window bound"); return; }
            doc_->screenshot(t[1], W_, H_, scale_, false);
            if (t.size() > 2 && t[2] == L"overlay")
                doc_->screenshot(t[1] + L".overlay.png", W_, H_, scale_, true);
        });
    addCommand(L"assert", 2, L"assert [not] exists|visible|text|value|caret|rect ...",
        [this](const auto& t, const auto&) { assertCmd(t); });
    addCommand(L"click-at", 3, L"click-at #id fx fy",
        [this](const auto& t, const auto&) {
            if (auto* e = need(t[1])) {
                layout();
                RectF r = e->box.borderRect;
                float x = r.x + r.w * (float)_wtof(t[2].c_str());
                float y = r.y + r.h * (float)_wtof(t[3].c_str());
                auto& ev = doc_->events();
                ev.onMouseMove(*doc_->root(), x, y, false);
                ev.onMouseDown(*doc_->root(), x, y);
                ev.onMouseUp(*doc_->root(), x, y);
                settle();
            }
        });
    addCommand(L"click-char", 2, L"click-char #id N",
        [this](const auto& t, const auto&) {
            if (auto* e = need(t[1])) {
                layout();
                auto* in = dynamic_cast<TextInput*>(e->control.get());
                if (!in) { fail(L"click-char: not a text input"); return; }
                int n = _wtoi(t[2].c_str());
                TextRun run = makeRun(in->value, e->style, scale_);   // REAL scale, same as paint
                auto offs = charOffsets(run);
                if (n < 0 || n >= (int)offs.size()) { fail(L"click-char: N out of range"); return; }
                RectF cr = e->box.contentRect();
                float x = cr.x - in->scrollX + offs[n];
                float y = cr.y + cr.h / 2;
                auto& ev = doc_->events();
                ev.onMouseMove(*doc_->root(), x, y, false);
                ev.onMouseDown(*doc_->root(), x, y);
                ev.onMouseUp(*doc_->root(), x, y);
                settle();
            }
        });
    addCommand(L"golden", 1, L"golden expected.layout.json [update]",
        [this](const auto& t, const auto&) {
            golden(t[1], t.size() > 2 && t[2] == L"update");
        });
    addCommand(L"include", 1, L"include <path>  (path relative to this script's dir)",
        [this](const auto& t, const auto&) { includeCmd(t); });
    // expect-fail <command...>: the inner command MUST fail. Cheap guard
    // against vacuous tests (an assertion that can't fire).
    addCommand(L"expect-fail", 1, L"expect-fail <command line that must fail>",
        [this](const auto& t, const std::wstring& line) {
            size_t pos = line.find(t[1]);
            if (pos == std::wstring::npos) { fail(L"expect-fail: malformed"); return; }
            std::wstring sub = line.substr(pos);   // from the subcommand word
            int before = failures_;
            run(sub);
            int delta = failures_ - before;
            if (delta == 0)
                fail(L"expect-fail: command did not fail: " + sub);
            else {
                // The inner failure is expected: undo its count and report
                // it as a note, not a failure.
                failures_ = before;
                messages_.erase(std::prev(messages_.end(), delta), messages_.end());
                note(L"expect-fail ok: " + sub + L" failed as required");
            }
        });
}

void ScriptRunner::setScriptDir(const std::wstring& dir) {
    scriptDir_ = dir;
}

std::wstring ScriptRunner::resolvePath(const std::wstring& path) const {
    bool absolute = (path.size() >= 2 && iswalpha(path[0]) && path[1] == L':')
        || (path.size() >= 1 && (path[0] == L'/' || path[0] == L'\\'));
    if (!scriptDir_.empty() && !absolute)
        return scriptDir_ + L"\\" + path;
    return path;
}

void ScriptRunner::golden(const std::wstring& path, bool update) {
    if (!doc_) { fail(L"golden: no window bound"); return; }
    layout();
    auto actual = doc_->layoutSnapshot();
    // Updating is deliberate: a runner flag (-Update sets UPDATE_GOLDEN=1),
    // not a hand-edited steps line.
    if (std::getenv("UPDATE_GOLDEN") != nullptr)
        update = true;
    std::wstring p = resolvePath(path);
    std::ifstream f(std::string(wideToUtf8(p)));
    if (!f) {
        if (update) {
            std::ofstream out(std::string(wideToUtf8(p)), std::ios::binary);
            out << actual.dump(1);
            return;
        }
        fail(L"golden: cannot open " + p + L" (run with -Update to record)");
        return;
    }

    auto expected = nlohmann::json::parse(f, nullptr, false);
    if (expected.is_discarded()) { fail(L"golden: cannot parse " + path); return; }

    for (auto& [k, e] : expected.items()) {
        if (!actual.contains(k)) { fail(L"golden: missing " + utf8ToWide(k)); continue; }
        // Shape check: a hand-edited golden must be an array of 4 numbers,
        // otherwise report a failure instead of throwing.
        if (!e.is_array() || e.size() != 4
            || !std::all_of(e.begin(), e.end(), [](auto& x) { return x.is_number(); })) {
            fail(L"golden: " + utf8ToWide(k) + L" is not an array of 4 numbers");
            continue;
        }
        for (int i = 0; i < 4; ++i)
            if (std::fabs(e[i].get<float>() - actual[k][i].get<float>()) > 0.5f) {
                fail(utf8ToWide(k) + L" expected " + utf8ToWide(e.dump()) +
                     L" got " + utf8ToWide(actual[k].dump()));
                break;
            }
    }
    for (auto& [k, v] : actual.items())
        if (!expected.contains(k)) fail(L"golden: unexpected " + utf8ToWide(k));
}

void ScriptRunner::run(const std::wstring& line) {
    auto t = tokens(line);
    if (t.empty() || t[0][0] == L'#') return;
    auto it = cmds_.find(t[0]);
    if (it == cmds_.end()) { fail(L"unknown command " + t[0]); return; }
    const Cmd& c = it->second;
    if (t.size() - 1 < c.minArgs) { fail(it->first + L": " + c.usage); return; }
    c.fn(t, line);

    if (doc_) {
        // The step may have destroyed the host that owns the bound document
        // (e.g. info #close -> start_countdown -> reset_word_windows):
        // unbind and skip validation of a dead document instead of running
        // against freed memory.
        if (alive_ && !alive_())
            unbind();
        else
            for (auto& p : doc_->validate())
                fail(L"validate: " + p);
    }
}

} // namespace ui
