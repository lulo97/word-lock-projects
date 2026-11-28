#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

TEST_CASE("Executable embeds app icon resource for Explorer", "[icon][resource]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    // ExtractIconEx returns the number of icon resources in the file.
    const UINT iconCount = ExtractIconExW(app.c_str(), 0, nullptr, nullptr, 0);
    REQUIRE(iconCount > 0);
}

TEST_CASE("favicon.ico is copied next to executable", "[icon][assets]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app.parent_path() / L"favicon.ico"));
}
