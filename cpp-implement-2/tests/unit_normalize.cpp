#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "app.h"

using namespace wordlock;

TEST_CASE("Countdown seconds are normalized to a safe range", "[unit][normalize]") {
    REQUIRE(NormalizeCountdownSeconds(12.4) == 12);
    REQUIRE(NormalizeCountdownSeconds(12.5) == 13);
    REQUIRE(NormalizeCountdownSeconds(0.0) == 1);
    REQUIRE(NormalizeCountdownSeconds(-3.0) == 1);
    REQUIRE(NormalizeCountdownSeconds(9999.0) == 3600);
    REQUIRE(NormalizeCountdownSeconds(std::numeric_limits<double>::quiet_NaN()) == 60);
}

TEST_CASE("Volume is quantized to half steps in the range 0..4", "[unit][normalize]") {
    REQUIRE(NormalizeVolume(0.6) == 0.5);
    REQUIRE(NormalizeVolume(1.3) == 1.5);
    REQUIRE(NormalizeVolume(-1.0) == 0.0);
    REQUIRE(NormalizeVolume(9.0) == 4.0);
    REQUIRE(NormalizeVolume(std::numeric_limits<double>::infinity()) == 1.0);
}

TEST_CASE("Theme ids normalize and cycle deterministically", "[unit][normalize]") {
    REQUIRE(ThemeIds().size() == 10);
    REQUIRE(NormalizeTheme(L"midnight") == L"midnight");
    REQUIRE(NormalizeTheme(L"unknown") == L"paper");
    REQUIRE(NextThemeId(L"paper") == L"brutal");
    REQUIRE(NextThemeId(L"zen") == L"paper");
    REQUIRE(NextThemeId(L"unknown") == L"brutal");  // Unknown starts at paper.
}
