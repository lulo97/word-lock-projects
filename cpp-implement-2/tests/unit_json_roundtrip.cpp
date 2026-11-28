#include <catch2/catch_test_macros.hpp>

#include "app.h"

using namespace wordlock;

TEST_CASE("Ignore words with quotes and backslashes survive a JSON round trip",
          "[unit][json][regression]") {
    // ParseIgnoreWords accepts these (no whitespace, no commas), so users can save them.
    const std::vector<std::wstring> accepted = ParseIgnoreWords(L"a\"b,c\\d,plain", false);
    REQUIRE(accepted.size() == 3);

    // This is exactly what SaveSettings writes, and what LoadSettings reads back.
    const std::wstring json = L"{\"english\":" + JsonStringArray(accepted) + L"}";

    std::vector<std::wstring> loaded;
    REQUIRE(TryGetStringArrayField(json, L"english", loaded));
    REQUIRE(loaded == accepted);
}