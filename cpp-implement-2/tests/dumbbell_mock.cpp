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

bool WaitForAllMessages(const std::filesystem::path& path,
                        const std::vector<std::string>& needles,
                        DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const std::string contents = ReadFile(path);
        bool allFound = true;
        for (const std::string& needle : needles) {
            if (contents.find(needle) == std::string::npos) {
                allFound = false;
                break;
            }
        }
        if (allFound) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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

TEST_CASE("Dumbbell mock uses heavy model and advances reps",
          "[dumbbell][mediapipe][mock][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const std::filesystem::path assetDir = app.parent_path();
    const std::filesystem::path model = assetDir / L"pose_landmarker_heavy.task";
    REQUIRE(std::filesystem::exists(model));
    REQUIRE(std::filesystem::file_size(model) > 1000000);
    REQUIRE(std::filesystem::exists(assetDir / L"mediapipe/vision_bundle.mjs"));
    REQUIRE(std::filesystem::exists(
        assetDir / L"mediapipe/wasm/vision_wasm_internal.wasm"));

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
    const std::filesystem::path testDir =
        TempRoot() / (L"PomodoroLockDumbbellMock_" + std::to_wstring(GetCurrentProcessId()) +
                      L"_" + std::to_wstring(stamp));
    const std::filesystem::path dumpFile = testDir / L"dumbbell_dom.json";
    const std::filesystem::path commandFile = testDir / L"command.js";
    const std::filesystem::path messageLog = testDir / L"messages.log";
    std::filesystem::create_directories(testDir);

    {
        std::ofstream command(commandFile, std::ios::binary | std::ios::trunc);
        command << R"JS((function () {
  var seen = {};
  function signal(type) {
    if (!seen[type]) { seen[type] = true; window.app.send(type); }
  }
  if (window.isSecureContext && window.location.protocol === 'https:') {
    signal('dumbbell:secure-origin');
  }
  if (navigator.mediaDevices &&
      typeof navigator.mediaDevices.getUserMedia === 'function') {
    signal('dumbbell:media-devices');
  }
  fetch('mediapipe/vision_bundle.mjs', { method: 'HEAD' }).then(function (response) {
    var contentType = response.headers.get('content-type') || '';
    if (response.ok && contentType.indexOf('javascript') >= 0) {
      signal('dumbbell:mjs-mime-ok');
    }
  }).catch(function () {});
  var timer = setInterval(function () {
    var mock = window.__dumbbellMock;
    if (mock && mock.modelAssetPath === 'pose_landmarker_heavy.task') {
      signal('dumbbell:model-heavy');
    }
    if (mock && mock.detectCount > 2) signal('dumbbell:mock-detecting');
    var left = document.getElementById('left-reps');
    var right = document.getElementById('right-reps');
    if (left && left.textContent !== '0 / 10') signal('dumbbell:reps-advancing');
    if (right && right.textContent !== '0 / 10') signal('dumbbell:both-arms');
    if (seen['dumbbell:secure-origin'] && seen['dumbbell:media-devices'] &&
        seen['dumbbell:mjs-mime-ok'] && seen['dumbbell:model-heavy'] &&
        seen['dumbbell:mock-detecting'] && seen['dumbbell:reps-advancing'] &&
        seen['dumbbell:both-arms']) {
      clearInterval(timer);
    }
  }, 50);
})();)JS";
    }

    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MODE", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_USE_TEST_DATA", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_CHROME_FREEZE_MOCK", L"1"));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_HOME", testDir.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_DOM_DUMP_FILE", dumpFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_COMMAND_FILE", commandFile.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_TEST_MESSAGE_LOG_FILE", messageLog.c_str()));
    REQUIRE(SetEnvironmentVariableW(L"WORDLOCK_DUMBBELL_MOCK", L"1"));

    std::wstring commandLine = L"\"" + app.wstring() + L"\" --test-open-dumbbell";
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ProcessGuard guard;
    REQUIRE(CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &startup, &guard.process));

    const std::wstring mainClass =
        L"MainWindowTest" + std::to_wstring(guard.process.dwProcessId);
    REQUIRE(WaitForVisibleWindow(mainClass, 2000) != nullptr);
    REQUIRE(WaitForNonEmptyFile(dumpFile, 2000));

    const std::vector<std::string> requiredMessages{
        "dumbbell:secure-origin", "dumbbell:media-devices", "dumbbell:mjs-mime-ok",
        "dumbbell:model-heavy", "dumbbell:mock-detecting",
        "dumbbell:reps-advancing", "dumbbell:both-arms"};
    REQUIRE(WaitForAllMessages(messageLog, requiredMessages, 2500));

    INFO("Dumbbell DOM dump: " << ReadFile(dumpFile));
    INFO("Host messages: " << ReadFile(messageLog));

    HWND mainWindow = FindWindowW(mainClass.c_str(), nullptr);
    REQUIRE(mainWindow != nullptr);
    REQUIRE(PostMessageW(mainWindow, kTestQuitMessage, 0, 0));
    REQUIRE(WaitForSingleObject(guard.process.hProcess, 1000) == WAIT_OBJECT_0);
    guard.Dismiss();
}
