#include <catch2/catch_test_macros.hpp>

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr UINT kTestQuitMessage = WM_APP + 2;

std::filesystem::path CurrentExecutablePath() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(std::wstring(buffer, length));
}

std::filesystem::path TempRoot() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, buffer);
    return std::filesystem::path(std::wstring(buffer, length));
}

std::filesystem::path AppExecutablePath() {
    const std::filesystem::path testExe = CurrentExecutablePath();
    const std::filesystem::path configDir = testExe.parent_path();
    const std::filesystem::path buildDir = configDir.parent_path().parent_path();
    return buildDir / configDir.filename() / L"PomodoroLockCpp.exe";
}

bool WaitForNonEmptyFile(const std::filesystem::path& path, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        if (std::filesystem::exists(path, error) &&
            std::filesystem::file_size(path, error) > 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

HWND WaitForVisibleWindow(const std::wstring& className, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        HWND window = FindWindowW(className.c_str(), nullptr);
        if (window != nullptr && IsWindowVisible(window)) {
            return window;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return nullptr;
}

struct ProcessGuard {
    PROCESS_INFORMATION process{};

    ProcessGuard() = default;
    ProcessGuard(const ProcessGuard&) = delete;
    ProcessGuard& operator=(const ProcessGuard&) = delete;

    ~ProcessGuard() {
        if (process.hProcess != nullptr) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(process.hProcess, &exitCode) &&
                exitCode == STILL_ACTIVE) {
                TerminateProcess(process.hProcess, 1);
            }
            CloseHandle(process.hProcess);
        }
        if (process.hThread != nullptr) {
            CloseHandle(process.hThread);
        }
    }

    void Dismiss() {
        process = {};
    }
};

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("Settings window renders Settings text at runtime",
          "[settings][runtime][webview]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockCppTests_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                      std::to_wstring(stamp));
    const std::filesystem::path dumpFile = testDir / L"settings_dom.json";
    std::filesystem::create_directories(testDir);

    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_HOME", testDir.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_DOM_DUMP_FILE", dumpFile.c_str()));

    std::wstring command = L"\"" + app.wstring() + L"\" --test-open-settings";
    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ProcessGuard guard;
    REQUIRE(CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &startup, &guard.process));

    const std::wstring settingClass =
        L"SettingWindowsTest" + std::to_wstring(guard.process.dwProcessId);
    const std::wstring mainClass =
        L"MainWindowTest" + std::to_wstring(guard.process.dwProcessId);

    HWND settingsWindow = WaitForVisibleWindow(settingClass, 3000);
    REQUIRE(settingsWindow != nullptr);
    REQUIRE(IsWindowVisible(settingsWindow));

    REQUIRE(WaitForNonEmptyFile(dumpFile, 5000));
    const std::string domDump = ReadFile(dumpFile);
    INFO("Settings WebView DOM dump: " << domDump);
    REQUIRE(domDump.find("Settings") != std::string::npos);

    HWND mainWindow = FindWindowW(mainClass.c_str(), nullptr);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(PostMessageW(mainWindow, kTestQuitMessage, 0, 0));
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
}
