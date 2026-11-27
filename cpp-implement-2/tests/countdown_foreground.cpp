#include <catch2/catch_test_macros.hpp>

#include <windows.h>

#include <chrono>
#include <filesystem>
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

LRESULT CALLBACK DistractorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void PumpMessages(DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ForceForeground(HWND hwnd) {
    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread = GetWindowThreadProcessId(foreground, nullptr);
    const DWORD currentThread = GetCurrentThreadId();
    const bool attached = foregroundThread != 0 && foregroundThread != currentThread &&
                          AttachThreadInput(currentThread, foregroundThread, TRUE);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    if (attached) {
        SetFocus(hwnd);
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
}

HWND CreateDistractor() {
    const std::wstring className =
        L"CountdownForegroundDistractor" + std::to_wstring(GetCurrentProcessId());
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = DistractorProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className.c_str();
    REQUIRE(RegisterClassW(&windowClass) != 0);

    HWND hwnd = CreateWindowExW(0, className.c_str(), L"Countdown Distractor",
                               WS_OVERLAPPEDWINDOW, 80, 80, 480, 320,
                               nullptr, nullptr, windowClass.hInstance, nullptr);
    REQUIRE(hwnd != nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return hwnd;
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

TEST_CASE("Countdown timeout brings the main window to the foreground",
          "[countdown][foreground][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockCountdownForeground_" +
                      std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(stamp));
    const std::filesystem::path messageLog = testDir / L"messages.log";
    std::filesystem::create_directories(testDir);

    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COUNTDOWN_SECONDS", L"2"));
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
    const std::wstring countdownClass =
        L"CountDownWindowTest" + std::to_wstring(guard.process.dwProcessId);
    HWND countdown = WaitForWindow(countdownClass, true, 3000);
    REQUIRE(countdown != nullptr);
    HWND mainWindow = FindWindowW(mainClass.c_str(), nullptr);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(!IsWindowVisible(mainWindow));

    HWND distractor = CreateDistractor();
    ForceForeground(distractor);
    PumpMessages(100);
    REQUIRE(GetForegroundWindow() == distractor);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(4000);
    while (std::chrono::steady_clock::now() < deadline && !IsWindowVisible(mainWindow)) {
        PumpMessages(20);
    }
    REQUIRE(IsWindowVisible(mainWindow));
    REQUIRE(!IsWindowVisible(countdown));
    PumpMessages(100);

    INFO("Foreground after countdown: " << GetForegroundWindow() << " main: " << mainWindow);
    REQUIRE(GetForegroundWindow() == mainWindow);

    REQUIRE(PostMessageW(mainWindow, kTestQuitMessage, 0, 0));
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
    DestroyWindow(distractor);
}
