#include <catch2/catch_test_macros.hpp>

#include "app.h"

using namespace wordlock;

TEST_CASE("Trim handles wide whitespace", "[unit][parsing]") {
    REQUIRE(Trim(L"  hello  ") == L"hello");
    REQUIRE(Trim(L"\u3000hello\u3000") == L"hello");  // Ideographic space.
    REQUIRE(Trim(L"") == L"");
    REQUIRE(Trim(L"   ") == L"");
}

TEST_CASE("ParseIgnoreWords splits and validates comma-separated words",
          "[unit][parsing]") {
    REQUIRE(ParseIgnoreWords(L"", false).empty());
    REQUIRE(ParseIgnoreWords(L"  ", false).empty());

    const auto words = ParseIgnoreWords(L"apple, banana ,apple", false);
    REQUIRE(words == std::vector<std::wstring>{L"apple", L"banana", L"apple"});

    REQUIRE_THROWS(ParseIgnoreWords(L"apple\nbanana", false));
    REQUIRE_THROWS(ParseIgnoreWords(L"apple\rbanana", false));
    REQUIRE_THROWS(ParseIgnoreWords(L"apple,,banana", false));
    REQUIRE_THROWS(ParseIgnoreWords(L"apple banana", false));
    REQUIRE(ParseIgnoreWords(L"\u3000apple", false) ==
            std::vector<std::wstring>{L"apple"});
}

TEST_CASE("ParseIgnoreWords accepts Japanese script words", "[unit][parsing]") {
    // Regression: isspace() on a truncated wchar_t rejected multi-byte kanji.
    REQUIRE(ParseIgnoreWords(L"\u4e0a", true) ==
            std::vector<std::wstring>{L"\u4e0a"});
    REQUIRE(ParseIgnoreWords(L"\u4e0a,\u3055\u304f\u3089", true).size() == 2);
    REQUIRE(IsJapaneseScriptWord(L"\u685c"));
    REQUIRE_FALSE(IsJapaneseScriptWord(L"apple"));
    REQUIRE_FALSE(IsJapaneseScriptWord(L""));
    REQUIRE_THROWS(ParseIgnoreWords(L"apple", true));
}

TEST_CASE("UrlDecode handles percent escapes and plus signs", "[unit][parsing]") {
    REQUIRE(UrlDecode(L"a+b") == L"a b");
    REQUIRE(UrlDecode(L"%41%42") == L"AB");
    REQUIRE(UrlDecode(L"%2f%2F") == L"//");
    REQUIRE(UrlDecode(L"%ZZ") == L"%ZZ");
    REQUIRE(UrlDecode(L"a%2") == L"a%2");
    REQUIRE(HexDigit(L'g') == -1);
}
