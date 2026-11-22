#include "app/script.h"

#include "controls/text_input.h"
#include "core/utf.h"
#include "dom/tree_ops.h"

#include "json.hpp"

#include <cwchar>
#include <cmath>
#include <fstream>

namespace ui {

ScriptRunner::ScriptRunner(Document& doc, int w, int h, float scale) {
    bind(doc, w, h, scale);
}

void ScriptRunner::bind(Document& doc, int w, int h, float scale) {
    doc_ = &doc;
    W_ = w;
    H_ = h;
    scale_ = scale;
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
    if (!doc_)
        return nullptr;
    Element* e = doc_->byId(sel.substr(sel[0] == L'#' ? 1 : 0));
    if (!e) fail(L"no element " + sel);
    return e;
}

void ScriptRunner::fail(const std::wstring& msg) {
    ++failures_;
    messages_.push_back(L"FAIL line " + std::to_wstring(lineNo_) + L": " + msg);
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

void ScriptRunner::key(const std::wstring& name) {
    if (!doc_) return;
    auto& ev = doc_->events();
    Element& root = *doc_->root();
    if (name == L"Backspace") ev.onChar(root, L'\b');
    else {
        static const std::pair<const wchar_t*, UINT> map[] = {
            {L"Enter", VK_RETURN}, {L"Tab", VK_TAB}, {L"Escape", VK_ESCAPE},
            {L"Left", VK_LEFT}, {L"Right", VK_RIGHT}, {L"Up", VK_UP}, {L"Down", VK_DOWN},
            {L"Home", VK_HOME}, {L"End", VK_END}, {L"Delete", VK_DELETE}, {L"Space", VK_SPACE}};
        for (auto& m : map)
            if (name == m.first) { KeyEvent k{}; k.vk = m.second; ev.onKeyDown(root, k); }
    }
    settle();
}

// assert exists|visible #id   |   assert text|value #id == X   |   assert rect #id w>0
void ScriptRunner::assertCmd(const std::vector<std::wstring>& t) {
    if (!doc_) return;
    Element* e = need(t[2]);
    if (!e) return;
    const std::wstring& kind = t[1];
    layout();
    if (kind == L"exists") return;
    if (kind == L"visible") {
        if (e->style.display == Display::None || e->box.borderRect.w <= 0)
            fail(t[2] + L" not visible");
    } else if (kind == L"text" || kind == L"value") {
        std::wstring actual = kind == L"text" ? textContent(*e) : doc_->getValue(e->id);
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
    layout();
    auto actual = doc_->layoutSnapshot();
    std::wstring p = resolvePath(path);
    std::ifstream f(std::string(wideToUtf8(p)));
    if (!f) {
        if (update) {
            std::ofstream out(std::string(wideToUtf8(p)), std::ios::binary);
            out << actual.dump(1);
            return;
        }
        fail(L"golden: cannot open " + p);
        return;
    }

    auto expected = nlohmann::json::parse(f, nullptr, false);
    if (expected.is_discarded()) { fail(L"golden: cannot parse " + path); return; }

    for (auto& [k, e] : expected.items()) {
        if (!actual.contains(k)) { fail(L"golden: missing " + utf8ToWide(k)); continue; }
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
    ++lineNo_;
    if (!doc_) { fail(L"no window bound"); return; }

    auto t = tokens(line);
    if (t.empty() || t[0][0] == L'#') return;
    const std::wstring& c = t[0];

    if (c == L"size")  { swscanf_s(t[1].c_str(), L"%dx%d", &W_, &H_); }
    else if (c == L"dpi") { parseFloat(t[1], scale_); }
    else if (c == L"click") { if (auto* e = need(t[1])) mouse(*e, true); }
    else if (c == L"hover") { if (auto* e = need(t[1])) mouse(*e, false); }
    else if (c == L"focus") { if (auto* e = need(t[1])) { doc_->events().setFocus(e); settle(); } }
    else if (c == L"type")  {
        if (auto* e = need(t[1])) {
            doc_->events().setFocus(e);
            for (wchar_t ch : t[2]) doc_->events().onChar(*doc_->root(), ch);
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
        doc_->events().onWheel(*doc_->root(), (float)_wtof(t[1].c_str()), (float)_wtof(t[2].c_str()), _wtoi(t[3].c_str()));
        settle();
    }
    else if (c == L"dump")  { layout(); doc_->dumpTree(t[1], 99); }
    else if (c == L"shot")  {
        doc_->screenshot(t[1], W_, H_, scale_, false);
        if (t.size() > 2 && t[2] == L"overlay")
            doc_->screenshot(t[1] + L".overlay.png", W_, H_, scale_, true);
    }
    else if (c == L"assert"){ assertCmd(t); }
    else if (c == L"golden") {
        if (t.size() < 2) fail(L"golden: missing path");
        else golden(t[1], t.size() > 2 && t[2] == L"update");
    }
    else fail(L"unknown command " + c);

    for (auto& p : doc_->validate())
        fail(L"validate: " + p);
}

} // namespace ui
