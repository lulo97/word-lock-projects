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

HWND WaitForWindow(const std::wstring& className, bool visible, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        HWND window = FindWindowW(className.c_str(), nullptr);
        if (window != nullptr && (!visible || IsWindowVisible(window))) {
            return window;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return nullptr;
}

bool WaitForFileContains(const std::filesystem::path& path,
                         const std::string& needle, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ReadFile(path).find(needle) != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

bool Near(int left, int right, int tolerance) {
    const int delta = left > right ? left - right : right - left;
    return delta <= tolerance;
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

void SetTestEnvironment(const std::filesystem::path& testDir,
                        const std::filesystem::path& messageLog) {
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_HOME", testDir.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_DOM_DUMP_FILE", L""));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COMMAND_FILE", L""));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MESSAGE_LOG_FILE", messageLog.c_str()));
}

void LaunchApp(const std::filesystem::path& app, ProcessGuard& guard) {
    std::wstring commandLine = L"\"" + app.wstring() + L"\"";
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    REQUIRE(CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &startup, &guard.process));
}

void QuitApp(ProcessGuard& guard, const std::wstring& mainClass) {
    HWND mainWindow = FindWindowW(mainClass.c_str(), nullptr);
    if (mainWindow != nullptr) {
        PostMessageW(mainWindow, kTestQuitMessage, 0, 0);
    }
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
}

void DragWindow(HWND hwnd, int deltaX, int deltaY) {
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    RECT rect{};
    REQUIRE(GetWindowRect(hwnd, &rect));
    const int startX = rect.left + 20;
    const int startY = rect.top + 20;
    REQUIRE(SetCursorPos(startX, startY));

    INPUT down{};
    down.type = INPUT_MOUSE;
    down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    REQUIRE(SendInput(1, &down, sizeof(INPUT)) == 1);

    for (int step = 1; step <= 5; ++step) {
        const int x = startX + deltaX * step / 5;
        const int y = startY + deltaY * step / 5;
        REQUIRE(SetCursorPos(x, y));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    INPUT up{};
    up.type = INPUT_MOUSE;
    up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    REQUIRE(SendInput(1, &up, sizeof(INPUT)) == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

}  // namespace

TEST_CASE("Countdown drags with a move cursor and remembers position until restart",
          "[countdown][drag][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockDrag_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                      std::to_wstring(stamp));
    const std::filesystem::path messageLog = testDir / L"messages.log";
    std::filesystem::create_directories(testDir);
    SetTestEnvironment(testDir, messageLog);

    ProcessGuard first;
    LaunchApp(app, first);
    const std::wstring firstMainClass = L"MainWindowTest" + std::to_wstring(first.process.dwProcessId);
    const std::wstring firstCountDownClass =
        L"CountDownWindowTest" + std::to_wstring(first.process.dwProcessId);
    HWND firstCountDown = WaitForWindow(firstCountDownClass, true, 3000);
    REQUIRE(firstCountDown != nullptr);
    REQUIRE(WaitForWindow(firstMainClass, false, 2000) != nullptr);

    RECT initial{};
    REQUIRE(GetWindowRect(firstCountDown, &initial));
    const int hitX = initial.left + 20;
    const int hitY = initial.top + 20;
    REQUIRE(SendMessageW(firstCountDown, WM_NCHITTEST, 0, MAKELPARAM(hitX, hitY)) == HTCAPTION);
    SendMessageW(firstCountDown, WM_SETCURSOR, 0, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    INFO("Countdown host messages: " << ReadFile(messageLog));
    REQUIRE(WaitForFileContains(messageLog, "countdown:cursor-sizeall", 2000));

    constexpr int deltaX = -150;
    constexpr int deltaY = -100;
    DragWindow(firstCountDown, deltaX, deltaY);

    RECT dragged{};
    REQUIRE(GetWindowRect(firstCountDown, &dragged));
    INFO("Initial countdown rect: " << initial.left << "," << initial.top);
    INFO("Dragged countdown rect: " << dragged.left << "," << dragged.top);
    REQUIRE(Near(dragged.left, initial.left + deltaX, 12));
    REQUIRE(Near(dragged.top, initial.top + deltaY, 12));

    HWND firstMain = FindWindowW(firstMainClass.c_str(), nullptr);
    REQUIRE(firstMain != nullptr);
    REQUIRE(SendMessageW(firstMain, WM_CLOSE, 0, 0) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    RECT afterRestartCountdown{};
    REQUIRE(GetWindowRect(firstCountDown, &afterRestartCountdown));
    REQUIRE(Near(afterRestartCountdown.left, dragged.left, 4));
    REQUIRE(Near(afterRestartCountdown.top, dragged.top, 4));
    QuitApp(first, firstMainClass);

    const std::filesystem::path secondDir = testDir / L"fresh-launch";
    const std::filesystem::path secondLog = secondDir / L"messages.log";
    std::filesystem::create_directories(secondDir);
    SetTestEnvironment(secondDir, secondLog);

    ProcessGuard second;
    LaunchApp(app, second);
    const std::wstring secondCountDownClass =
        L"CountDownWindowTest" + std::to_wstring(second.process.dwProcessId);
    HWND secondCountDown = WaitForWindow(secondCountDownClass, true, 3000);
    REQUIRE(secondCountDown != nullptr);

    RECT fresh{};
    REQUIRE(GetWindowRect(secondCountDown, &fresh));
    INFO("Fresh launch rect: " << fresh.left << "," << fresh.top);
    REQUIRE(Near(fresh.left, initial.left, 4));
    REQUIRE(Near(fresh.top, initial.top, 4));
    QuitApp(second, L"MainWindowTest" + std::to_wstring(second.process.dwProcessId));
}
