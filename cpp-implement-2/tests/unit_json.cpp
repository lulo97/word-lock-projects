#include <catch2/catch_test_macros.hpp>

#include "app.h"

using namespace wordlock;

TEST_CASE("JSON string fields are read with escape handling", "[unit][json]") {
    std::wstring value;
    REQUIRE(TryGetStringField(L"{\"theme\":\"midnight\"}", L"theme", value));
    REQUIRE(value == L"midnight");

    REQUIRE(TryGetStringField(L"{\"text\":\"a\\nb\\\"c\"}", L"text", value));
    REQUIRE(value == L"a\nb\"c");

    REQUIRE_FALSE(TryGetStringField(L"{\"theme\":42}", L"theme", value));
    REQUIRE_FALSE(TryGetStringField(L"{\"other\":\"x\"}", L"theme", value));
}

TEST_CASE("JSON number and bool fields fall back cleanly", "[unit][json]") {
    double number = 0.0;
    REQUIRE(TryGetNumberField(L"{\"countdownSeconds\": 12.5}", L"countdownSeconds", number));
    REQUIRE(number == 12.5);
    REQUIRE_FALSE(TryGetNumberField(L"{\"countdownSeconds\":\"12\"}",
                                    L"countdownSeconds", number));

    REQUIRE(TryGetBoolField(L"{\"autoTheme\":true}", L"autoTheme", false));
    REQUIRE_FALSE(TryGetBoolField(L"{\"autoTheme\":false}", L"autoTheme", true));
    REQUIRE(TryGetBoolField(L"{}", L"autoTheme", true));
}

TEST_CASE("JSON string arrays and escaping round-trip", "[unit][json]") {
    std::vector<std::wstring> values;
    REQUIRE(TryGetStringArrayField(L"{\"english\":[\"apple\", \"banana\"]}",
                                   L"english", values));
    REQUIRE(values == std::vector<std::wstring>{L"apple", L"banana"});
    REQUIRE(TryGetStringArrayField(L"{\"english\":[]}", L"english", values));
    REQUIRE(values.empty());
    REQUIRE_FALSE(TryGetStringArrayField(L"{\"english\":\"apple\"}", L"english", values));

    REQUIRE(JsonEscape(L"a\"b\\c\n\t") == L"a\\\"b\\\\c\\n\\t");
    REQUIRE(Utf8ToWide(WideToUtf8(L"\u685c")) == L"\u685c");
    REQUIRE(WideToUtf8(L"abc") == "abc");
}
