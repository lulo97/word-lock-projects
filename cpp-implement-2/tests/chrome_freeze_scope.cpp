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

constexpr UINT kTrayMessage = WM_APP + 1;
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

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void TruncateFile(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
}

int CountOccurrences(const std::string& text, const std::string& needle) {
    int count = 0;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

HWND WaitForWindow(const std::wstring& className, bool visible, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        HWND window = FindWindowW(className.c_str(), nullptr);
        if (window != nullptr && (!visible || IsWindowVisible(window))) {
            return window;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return nullptr;
}

bool WaitForCount(const std::filesystem::path& path, const std::string& needle,
                  int minimumCount, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (CountOccurrences(ReadFile(path), needle) >= minimumCount) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
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

    void Dismiss() { process = {}; }
};

}  // namespace

TEST_CASE("Chrome freeze follows main window visibility",
          "[chrome][freeze][mock][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockChromeFreeze_" + std::to_wstring(GetCurrentProcessId()) +
                      L"_" + std::to_wstring(stamp));
    const std::filesystem::path messageLog = testDir / L"messages.log";
    std::filesystem::create_directories(testDir);

    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_CHROME_FREEZE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_HOME", testDir.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_DOM_DUMP_FILE", L""));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COMMAND_FILE", L""));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MESSAGE_LOG_FILE", messageLog.c_str()));

    std::wstring commandLine = L"\"" + app.wstring() + L"\"";
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ProcessGuard guard;
    REQUIRE(CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &startup, &guard.process));

    const std::wstring mainClass =
        L"MainWindowTest" + std::to_wstring(guard.process.dwProcessId);
    HWND mainWindow = WaitForWindow(mainClass, false, 3000);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(WaitForCount(messageLog, "chrome:unfreeze", 1, 2000));

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    INFO("Mock log while main is hidden: " << ReadFile(messageLog));
    REQUIRE(ReadFile(messageLog).find("chrome:freeze") == std::string::npos);
    TruncateFile(messageLog);

    REQUIRE(PostMessageW(mainWindow, kTrayMessage, 0, WM_LBUTTONUP));
    REQUIRE(WaitForCount(messageLog, "chrome:freeze", 1, 3000));
    REQUIRE(WaitForWindow(mainClass, true, 2000) != nullptr);
    REQUIRE(WaitForCount(messageLog, "chrome:freeze", 2, 2500));

    TruncateFile(messageLog);
    REQUIRE(PostMessageW(mainWindow, WM_CLOSE, 0, 0));
    REQUIRE(WaitForCount(messageLog, "chrome:unfreeze", 1, 2000));
    REQUIRE(!IsWindowVisible(mainWindow));

    std::this_thread::sleep_for(std::chrono::milliseconds(2300));
    const std::string afterClose = ReadFile(messageLog);
    INFO("Mock log after main closes: " << afterClose);
    REQUIRE(afterClose.find("chrome:freeze") == std::string::npos);
    REQUIRE(CountOccurrences(afterClose, "chrome:unfreeze") == 1);

    REQUIRE(PostMessageW(mainWindow, kTestQuitMessage, 0, 0));
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
}
