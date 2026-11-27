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

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

bool WaitForFileContains(const std::filesystem::path& path,
                         const std::string& needle, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ReadFile(path).find(needle) != std::string::npos) {
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
            if (GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
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

TEST_CASE("Swapped theme survives the next task navigation",
          "[theme][settings][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockThemeSwap_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                      std::to_wstring(stamp));
    const std::filesystem::path dumpFile = testDir / L"task_dom.json";
    const std::filesystem::path commandFile = testDir / L"command.js";
    const std::filesystem::path messageLog = testDir / L"messages.log";
    std::filesystem::create_directories(testDir);

    {
        std::ofstream command(commandFile, std::ios::binary | std::ios::trunc);
        command << "(function () { var key = 'wordlockThemeSwapConfirmed_" << stamp << "';"
                << R"JS(
  var theme = document.documentElement && document.documentElement.dataset.theme
    ? document.documentElement.dataset.theme : '';
  window.app.send('theme:navigation:' + theme);

  var confirmed = false;
  try {
    confirmed = localStorage.getItem(key) === '1';
  } catch (error) { /* storage may be unavailable */ }

  if (confirmed) {
    window.app.send('theme:final:' + theme);
    return;
  }

  if (document.getElementById('answer')) {
    try { localStorage.setItem(key, '1'); } catch (error) {}
    window.app.call('settings:confirm', { theme: 'midnight' });
  }
})();)JS";
    }

    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COUNTDOWN_SECONDS", L"2"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_HOME", testDir.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_DOM_DUMP_FILE", dumpFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COMMAND_FILE", commandFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MESSAGE_LOG_FILE", messageLog.c_str()));

    std::wstring commandLine = L"\"" + app.wstring() + L"\" --test-open-english";
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ProcessGuard guard;
    REQUIRE(CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &startup, &guard.process));

    const std::wstring mainClass =
        L"MainWindowTest" + std::to_wstring(guard.process.dwProcessId);
    HWND mainWindow = nullptr;
    const auto windowDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (std::chrono::steady_clock::now() < windowDeadline) {
        mainWindow = FindWindowW(mainClass.c_str(), nullptr);
        if (mainWindow != nullptr && IsWindowVisible(mainWindow)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    REQUIRE(mainWindow != nullptr);
    REQUIRE(IsWindowVisible(mainWindow));

    REQUIRE(WaitForFileContains(messageLog, "settings:confirm", 4000));
    REQUIRE(WaitForFileContains(messageLog, "theme:final:", 5000));

    const std::string messages = ReadFile(messageLog);
    INFO("Host messages: " << messages);
    REQUIRE(messages.find("theme:final:midnight") != std::string::npos);
    REQUIRE(messages.find("theme:final:paper") == std::string::npos);

    REQUIRE(PostMessageW(mainWindow, kTestQuitMessage, 0, 0));
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
}
