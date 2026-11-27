#include <catch2/catch_test_macros.hpp>

#include <windows.h>

#include <filesystem>
#include <string>

namespace {

std::filesystem::path CurrentExecutablePath() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(std::wstring(buffer, length));
}

std::filesystem::path AppExecutablePath() {
    const std::filesystem::path testExe = CurrentExecutablePath();
    const std::filesystem::path configDir = testExe.parent_path();
    const std::filesystem::path buildDir = configDir.parent_path().parent_path();
    return buildDir / configDir.filename() / L"PomodoroLockCpp.exe";
}

}  // namespace

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
