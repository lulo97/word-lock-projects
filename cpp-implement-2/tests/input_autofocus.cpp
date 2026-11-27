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

bool WaitForFileContains(const std::filesystem::path& path, const std::string& needle,
                         DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ReadFile(path).find(needle) != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
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
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
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

    void Dismiss() { process = {}; }
};

void TypeAbc(HWND focusedWindow) {
    for (wchar_t character : std::wstring(L"abc")) {
        REQUIRE(PostMessageW(focusedWindow, WM_CHAR, static_cast<WPARAM>(character), 1));
    }
}

void RunAutofocusScenario(const std::filesystem::path& app,
                          const std::string& languageTag,
                          const std::wstring& launchArg) {
    const std::wstring languageWide(languageTag.begin(), languageTag.end());
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockAutofocus_" + languageWide + L"_" +
                      std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(stamp));
    const std::filesystem::path dumpFile = testDir / L"autofocus_dom.json";
    const std::filesystem::path commandFile = testDir / L"command.js";
    const std::filesystem::path messageLog = testDir / L"messages.log";
    std::filesystem::create_directories(testDir);

    {
        std::ofstream command(commandFile, std::ios::binary | std::ios::trunc);
        command << R"JS((function () {
  var signaled = false;
  var timer = setInterval(function () {
    var active = document.activeElement;
    if (!signaled && active && active.id === 'answer') {
      signaled = true;
      window.app.send('input:autofocused');
    }
    var answer = document.getElementById('answer');
    if (answer && answer.value.toLowerCase() === 'abc') {
      clearInterval(timer);
      window.app.send('input:typed:abc');
    }
  }, 40);
  setTimeout(function () {
    if (!signaled) {
      clearInterval(timer);
      window.app.send('input:autofocus-failed');
    }
  }, 2500);
})();)JS";
    }

    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_HOME", testDir.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_DOM_DUMP_FILE", dumpFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COMMAND_FILE", commandFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MESSAGE_LOG_FILE", messageLog.c_str()));

    std::wstring commandLine = L"\"" + app.wstring() + L"\" " + launchArg;
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ProcessGuard guard;
    REQUIRE(CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &startup, &guard.process));

    const std::wstring mainClass =
        L"MainWindowTest" + std::to_wstring(guard.process.dwProcessId);
    HWND mainWindow = WaitForVisibleWindow(mainClass, 4000);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(WaitForFileContains(messageLog, "input:autofocused", 3000));
    INFO(languageTag << " host messages before typing: " << ReadFile(messageLog));

    DWORD appPid = 0;
    const DWORD appThread = GetWindowThreadProcessId(mainWindow, &appPid);
    GUITHREADINFO focusInfo{};
    focusInfo.cbSize = sizeof(focusInfo);
    const BOOL focusRead = GetGUIThreadInfo(appThread, &focusInfo);
    const bool focusIsChild = focusRead && IsChild(mainWindow, focusInfo.hwndFocus);
    INFO("foreground=" << GetForegroundWindow() << " main=" << mainWindow
                       << " focus=" << (focusRead ? focusInfo.hwndFocus : nullptr)
                       << " focusIsChild=" << focusIsChild);
    REQUIRE(focusIsChild);

    TypeAbc(focusInfo.hwndFocus);

    const bool typed = WaitForFileContains(messageLog, "input:typed:abc", 2500);
    INFO(languageTag << " host messages after typing: " << ReadFile(messageLog));
    REQUIRE(typed);
    REQUIRE(ReadFile(messageLog).find("input:autofocus-failed") == std::string::npos);

    REQUIRE(PostMessageW(mainWindow, kTestQuitMessage, 0, 0));
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
}

}  // namespace

TEST_CASE("Word input is focused and ready for typing when main opens",
          "[input][focus][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));
    RunAutofocusScenario(app, "english", L"--test-open-english");
    RunAutofocusScenario(app, "japanese", L"--test-open-japanese");
}
