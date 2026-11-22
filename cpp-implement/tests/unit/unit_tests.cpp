// unit_tests.cpp
//
// Table-driven unit tests for the pure engine functions. No test dependency:
// a couple of CHECK macros and a main() that runs every test.
//
// Build:  cmake --build build --config Release   (target: unit_tests)
//
// Convention: add one test per bug you fix, named after the bug.

#include "core/utf.h"
#include "dom/tree_ops.h"
#include "parser/html_parser.h"
#include "style/class_rules.h"
#include "style/palette.h"
#include "style/spacing.h"
#include "style/variants.h"
#include "svg/path.h"
#include "../../utils/word_db.h"

#include <iostream>

using namespace ui;

static int g_fail = 0, g_run = 0;

#define CHECK(c) do { ++g_run; if (!(c)) { ++g_fail; std::cout << __FILE__ << ":" << __LINE__ << "  " #c "\n"; } } while (0)
#define CHECK_EQ(a, b) do { ++g_run; auto _a = (a); auto _b = (b); if (!(_a == _b)) { ++g_fail; std::cout << __FILE__ << ":" << __LINE__ << "  " #a " != " #b "\n"; } } while (0)
#define WCHECK_EQ(a, b) do { ++g_run; auto _a = (a); auto _b = (b); if (!(_a == _b)) { ++g_fail; std::wcout << __LINE__ << L"  " L#a L" != " L#b << L"  [" << _a << L"] [" << _b << L"]\n"; } } while (0)

void test_spacing() {
    float px;
    bool pct;
    CHECK(parseSpacingScale(L"4", px) && px == 16);
    CHECK(parseSpacingScale(L"0.5", px) && px == 2);
    CHECK(!parseSpacingScale(L"13", px));                       // not in scale
    CHECK(parseArbitraryLength(L"[1.5rem]", 16, px, pct) && px == 24 && !pct);
    CHECK(parseArbitraryLength(L"[50%]", 16, px, pct) && pct && px == 50);
    CHECK(!parseArbitraryLength(L"[abc]", 16, px, pct));
    Length l;
    CHECK(parseSizeToken(L"1/2", l) && l.unit == Length::Unit::Percent && l.value == 50);
}

void test_color() {
    Color c;
    CHECK(parseColorToken(L"blue-500", c) && c.r == 59 && c.g == 130 && c.b == 246);
    CHECK(parseColorToken(L"blue-500/50", c) && c.a == 127);
    CHECK(parseCssColor(L"#fff", c) && c.r == 255);
    CHECK(parseCssColor(L"#ff000080", c) && c.a == 128);
    CHECK(!parseCssColor(L"#ff", c));
    CHECK(!parseColorToken(L"blue-555", c));
}

void test_classes() {
    Declarations d;
    CHECK(applyGenericClass(L"px-4", d));
    CHECK(!applyGenericClass(L"px-13", d));
    CHECK(!applyGenericClass(L"bg-slate-950/50x", d));
    // Flags the unsupported-variant-apply-unconditionally bug (#2).
    CHECK(findClassIssues({ L"dark:bg-slate-900" }).size() == 1);
    CHECK(findClassIssues({ L"hover:bg-blue-500", L"flex" }).empty());
}

void test_utf() {
    WCHECK_EQ(decodeEntities(L"a&amp;b&#x41;&#66;"), std::wstring(L"a&bAB"));
    WCHECK_EQ(decodeEntities(L"&bogus; &"), std::wstring(L"&bogus; &"));
    CHECK_EQ(wideToUtf8(utf8ToWide(std::string("こんにちは"))), std::string("こんにちは"));
    WCHECK_EQ(trim(L"  x \n"), std::wstring(L"x"));
    float f;
    CHECK(parseFloat(L" 1.5 ", f) && f == 1.5f);
    CHECK(!parseFloat(L"1.5x", f));
}

void test_parser() {
    auto r = parseHtml(L"<div id=a class='x y'><input value=\"1\"><br><span>hi</span></div>");
    Element* a = findById(*r, L"a");
    CHECK(a && a->classList.size() == 2 && a->children.size() == 3);
    auto r2 = parseHtml(L"<p>unclosed<div>x");                    // must not crash
    (void)r2;
    auto r3 = parseHtml(L"<style>div{}</style><!-- c --><b>ok</b>");
    CHECK(findByTag(*r3, L"style").empty());
}

void test_path() {
    Path p;
    CHECK(parsePathData(L"M0 0 L10 10 Z", p) && p.segs.size() == 3);
    CHECK(parsePathData(L"M0 0 a5 5 0 1 1 10 0", p) && p.segs.size() >= 2);
    CHECK(!parsePathData(L"", p));
    CHECK(!parsePathData(L"M 1", p) || p.segs.size() <= 1);       // malformed: no crash
}

void test_template() {
    WordData w{};
    w.kanji = L"食べる";
    WCHECK_EQ(fill_template(L"{{kanji}}!", w), std::wstring(L"食べる!"));
    // Documents current behaviour of the unresolved-placeholder bug (#3).
    std::wstring out = fill_template(L"{{example3_vi}}", w);
    CHECK(out.find(L"{{") != std::wstring::npos);                // flip once you decide to strip
}

int main() {
    test_spacing();
    test_color();
    test_classes();
    test_utf();
    test_parser();
    test_path();
    test_template();
    std::cout << g_run << " checks, " << g_fail << " failure(s)\n";
    return g_fail ? 1 : 0;
}
