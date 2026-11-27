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

bool NearChannel(int left, int right, int tolerance = 12) {
    const int delta = left > right ? left - right : right - left;
    return delta <= tolerance;
}

}  // namespace

TEST_CASE("Countdown repaints with the active theme and keeps layered dragging",
          "[countdown][theme][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockCountdownTheme_" + std::to_wstring(GetCurrentProcessId()) +
                      L"_" + std::to_wstring(stamp));
    const std::filesystem::path dumpFile = testDir / L"settings_dom.json";
    const std::filesystem::path commandFile = testDir / L"command.js";
    const std::filesystem::path messageLog = testDir / L"messages.log";
    std::filesystem::create_directories(testDir);

    {
        std::ofstream command(commandFile, std::ios::binary | std::ios::trunc);
        command << "window.app.send('theme:preview', { value: 'midnight' });";
    }

    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_HOME", testDir.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_DOM_DUMP_FILE", dumpFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COMMAND_FILE", commandFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MESSAGE_LOG_FILE", messageLog.c_str()));

    std::wstring commandLine = L"\"" + app.wstring() + L"\" --test-open-settings";
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ProcessGuard guard;
    REQUIRE(CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &startup, &guard.process));

    const std::wstring countdownClass =
        L"CountDownWindowTest" + std::to_wstring(guard.process.dwProcessId);
    const std::wstring mainClass =
        L"MainWindowTest" + std::to_wstring(guard.process.dwProcessId);
    HWND countdown = WaitForVisibleWindow(countdownClass, 4000);
    REQUIRE(countdown != nullptr);

    const bool previewSent = WaitForFileContains(messageLog, "theme:preview", 4000);
    INFO("Host messages: " << ReadFile(messageLog));
    REQUIRE(previewSent);
    const bool themed = WaitForFileContains(messageLog, "countdown:theme:midnight", 3000);
    INFO("Host messages after preview: " << ReadFile(messageLog));
    REQUIRE(themed);

    const LONG_PTR exStyle = GetWindowLongPtrW(countdown, GWL_EXSTYLE);
    REQUIRE((exStyle & WS_EX_LAYERED) != 0);
    BYTE alpha = 0;
    DWORD flags = 0;
    REQUIRE(GetLayeredWindowAttributes(countdown, nullptr, &alpha, &flags));
    REQUIRE((flags & LWA_ALPHA) != 0);
    REQUIRE(alpha == 128);

    REQUIRE(SendMessageW(countdown, WM_NCHITTEST, 0,
                         MAKELPARAM(20, 20)) == HTCAPTION);

    HDC hdc = GetDC(countdown);
    REQUIRE(hdc != nullptr);
    const COLORREF background = GetPixel(hdc, 6, 6);
    ReleaseDC(countdown, hdc);
    INFO("Countdown background COLORREF: " << background);
    REQUIRE(NearChannel(GetRValue(background), 0x1e));
    REQUIRE(NearChannel(GetGValue(background), 0x29));
    REQUIRE(NearChannel(GetBValue(background), 0x3b));

    HWND mainWindow = FindWindowW(mainClass.c_str(), nullptr);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(PostMessageW(mainWindow, kTestQuitMessage, 0, 0));
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
}
