#include <catch2/catch_test_macros.hpp>

#include "app.h"

using namespace wordlock;

TEST_CASE("App resource URIs are parsed relative to the executable",
          "[unit][uri]") {
    REQUIRE(IsAppResourceUri(L"https://app/settings.html"));
    REQUIRE_FALSE(IsAppResourceUri(L"https://example.com/settings.html"));

    REQUIRE(AppRelativePathFromUri(L"https://app/settings.html?v=1") == L"settings.html");
    REQUIRE(AppRelativePathFromUri(L"https://app/ui/theme.css#top") == L"ui\\theme.css");
    REQUIRE(AppRelativePathFromUri(L"https://app/a%20b.js") == L"a b.js");
    REQUIRE(AppRelativePathFromUri(L"https://example.com/x").empty());
}

TEST_CASE("App resource URIs reject traversal outside the executable directory",
          "[unit][uri][security]") {
    REQUIRE(IsAppResourceUriAllowed(L"https://app/settings.html"));
    REQUIRE(IsAppResourceUriAllowed(L"https://app/ui/theme.css"));
    REQUIRE_FALSE(IsAppResourceUriAllowed(L"https://app/../secret"));
    REQUIRE_FALSE(IsAppResourceUriAllowed(L"https://app/%2e%2e/secret"));
    REQUIRE_FALSE(IsAppResourceUriAllowed(L"https://app/..\\secret"));

    // Non-app URIs are left to WebView2's normal handling.
    REQUIRE(IsAppResourceUriAllowed(L"https://example.com/secret"));
}
