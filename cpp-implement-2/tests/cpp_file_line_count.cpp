#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr int kMaxLines = 99;

std::filesystem::path RepositoryRoot() {
    // CTest runs from the build directory; running the exe directly may use
    // build/tests/<Config>. Walk up until the CMake cache is found.
    std::filesystem::path dir = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        if (std::filesystem::exists(dir / "CMakeCache.txt")) {
            return dir.parent_path();
        }
        if (!dir.has_parent_path()) {
            break;
        }
        dir = dir.parent_path();
    }
    return std::filesystem::current_path();
}

std::vector<std::filesystem::path> CppFilesIn(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(directory)) {
        return files;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".cpp") {
            continue;  // Only .cpp files are checked; other formats are skipped.
        }
        files.push_back(entry.path());
    }
    return files;
}

int CountLines(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    int lines = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++lines;
    }
    return lines;
}

}  // namespace

TEST_CASE("All src .cpp files are under 100 lines", "[structure][maintainability]") {
    const std::filesystem::path src = RepositoryRoot() / "src";
    const auto files = CppFilesIn(src);
    REQUIRE(!files.empty());

    for (const auto& file : files) {
        const int lines = CountLines(file);
        INFO(file.filename().string() << " has " << lines << " lines");
        REQUIRE(lines < 100);
    }
}
