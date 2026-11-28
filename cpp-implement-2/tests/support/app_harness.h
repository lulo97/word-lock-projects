#pragma once

#include <windows.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "app_messages.h"

// CMake supplies the exact app binary so tests never rebuild a stale exe.
#ifndef APP_EXE
#define APP_EXE ""
#endif

namespace test_support {

inline std::filesystem::path TempRoot() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, buffer);
    return std::filesystem::path(std::wstring(buffer, length));
}

inline std::filesystem::path AppExecutablePath() {
    const std::filesystem::path configured(APP_EXE);
    if (!configured.empty()) {
        return configured;
    }
    // Fallback for tests compiled without the CMake definition.
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    const std::filesystem::path configDir =
        std::filesystem::path(std::wstring(buffer, length)).parent_path();
    return configDir.parent_path().parent_path() / configDir.filename() / L"PomodoroLockCpp.exe";
}

inline void ReadFileInto(const std::filesystem::path& path, std::string& out) {
    out.clear();
    std::ifstream input(path, std::ios::binary);
    if (input) {
        out.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
}

inline std::string ReadFile(const std::filesystem::path& path) {
    std::string contents;
    ReadFileInto(path, contents);
    return contents;
}

inline void TruncateFile(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
}

inline int CountOccurrences(const std::string& text, const std::string& needle) {
    int count = 0;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

inline HWND WaitForWindow(const std::wstring& className, bool visible, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const HWND window = FindWindowW(className.c_str(), nullptr);
        if (window != nullptr && (!visible || IsWindowVisible(window))) {
            return window;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return nullptr;
}

inline bool WaitForFileContains(const std::filesystem::path& path,
                                const std::string& needle, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::string contents;
    contents.reserve(4096);
    while (std::chrono::steady_clock::now() < deadline) {
        ReadFileInto(path, contents);
        if (contents.find(needle) != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

inline bool WaitForFileCount(const std::filesystem::path& path, const std::string& needle,
                             int minimumCount, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::string contents;
    contents.reserve(4096);
    while (std::chrono::steady_clock::now() < deadline) {
        ReadFileInto(path, contents);
        if (CountOccurrences(contents, needle) >= minimumCount) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

inline bool WaitForNonEmptyFile(const std::filesystem::path& path, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        if (std::filesystem::exists(path, error) &&
            std::filesystem::file_size(path, error) > 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

inline bool WaitForAllMessages(const std::filesystem::path& path,
                               const std::vector<std::string>& needles, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::string contents;
    contents.reserve(4096);
    while (std::chrono::steady_clock::now() < deadline) {
        ReadFileInto(path, contents);
        bool allFound = true;
        for (const std::string& needle : needles) {
            if (contents.find(needle) == std::string::npos) {
                allFound = false;
                break;
            }
        }
        if (allFound) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

inline bool WaitForHiddenWindow(HWND window, DWORD timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (window == nullptr || !IsWindowVisible(window)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

struct TestApp {
    struct Options {
        std::wstring args;
        std::vector<std::pair<std::wstring, std::wstring>> env;
        std::string commandScript;
        bool wantDomDump = false;
        bool useTestData = true;
        bool chromeFreezeMock = true;
        bool singleInstance = false;
        int freezeIntervalMs = 0;   // 0 keeps the app default.
        int countdownTickMs = 0;    // 0 keeps the app default.
        std::wstring prefix = L"PomodoroLockTest";
        std::wstring singleInstanceName;  // Shared mutex suffix for instance tests.
        std::wstring testHome;            // Override WORDLOCK_TEST_HOME across restarts.
    };

    explicit TestApp(const Options& options = {}) {
        static std::atomic<int> sequence{0};
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch()).count();
        dir = TempRoot() / (options.prefix + L"_" + std::to_wstring(GetCurrentProcessId()) +
                            L"_" + std::to_wstring(stamp) + L"_" +
                            std::to_wstring(sequence.fetch_add(1)));
        messageLog = dir / L"messages.log";
        dumpFile = dir / L"dom_dump.json";
        const std::filesystem::path commandFile = dir / L"command.js";
        std::filesystem::create_directories(dir);

        if (!options.commandScript.empty()) {
            std::ofstream command(commandFile, std::ios::binary | std::ios::trunc);
            command << options.commandScript;
        }

        SetEnv(L"WORDLOCK_TEST_MODE", L"1");
        SetEnv(L"WORDLOCK_USE_TEST_DATA", options.useTestData ? L"1" : L"");
        SetEnv(L"WORDLOCK_CHROME_FREEZE_MOCK", options.chromeFreezeMock ? L"1" : L"");
        SetEnv(L"WORDLOCK_TEST_HOME", options.testHome.empty() ? dir.wstring() : options.testHome);
        // Command scripts run from the DOM-dump hook, so a script implies a dump.
        const bool needsDump = options.wantDomDump || !options.commandScript.empty();
        SetEnv(L"WORDLOCK_TEST_DOM_DUMP_FILE", needsDump ? dumpFile.wstring() : L"");
        SetEnv(L"WORDLOCK_TEST_COMMAND_FILE",
               options.commandScript.empty() ? L"" : commandFile.wstring());
        SetEnv(L"WORDLOCK_TEST_MESSAGE_LOG_FILE", messageLog.wstring());
        if (options.freezeIntervalMs > 0) {
            SetEnv(L"WORDLOCK_TEST_FREEZE_INTERVAL_MS", std::to_wstring(options.freezeIntervalMs));
        }
        if (options.countdownTickMs > 0) {
            SetEnv(L"WORDLOCK_TEST_COUNTDOWN_TICK_MS", std::to_wstring(options.countdownTickMs));
        }
        if (options.singleInstance) {
            SetEnv(L"WORDLOCK_TEST_SINGLE_INSTANCE", L"1");
        }
        if (!options.singleInstanceName.empty()) {
            SetEnv(L"WORDLOCK_TEST_SINGLE_INSTANCE_NAME", options.singleInstanceName);
        }
        for (const auto& entry : options.env) {
            SetEnv(entry.first, entry.second);
        }

        std::wstring command = L"\"" + AppExecutablePath().wstring() + L"\"";
        if (!options.args.empty()) {
            command += L" " + options.args;
        }
        std::vector<wchar_t> buffer(command.begin(), command.end());
        buffer.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        REQUIRE(CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                               nullptr, &startup, &process));

        // Killing the job on close also reaps WebView2's child processes.
        job = CreateJobObjectW(nullptr, nullptr);
        if (job != nullptr) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                                    sizeof(limits));
            AssignProcessToJobObject(job, process.hProcess);
        }

        const std::wstring suffix = L"Test" + std::to_wstring(process.dwProcessId);
        mainClass = L"MainWindow" + suffix;
        countdownClass = L"CountDownWindow" + suffix;
        settingClass = L"SettingWindows" + suffix;
        ignoreClass = L"IgnoreWordsWindows" + suffix;
    }

    TestApp(const TestApp&) = delete;
    TestApp& operator=(const TestApp&) = delete;

    ~TestApp() {
        if (process.hProcess != nullptr) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                TerminateProcess(process.hProcess, 1);
            }
        }
        if (job != nullptr) {
            CloseHandle(job);
        }
        if (process.hProcess != nullptr) {
            CloseHandle(process.hProcess);
        }
        if (process.hThread != nullptr) {
            CloseHandle(process.hThread);
        }
        for (auto it = originalEnv_.rbegin(); it != originalEnv_.rend(); ++it) {
            SetEnvironmentVariableW(it->first.c_str(),
                                    it->second.empty() ? nullptr : it->second.c_str());
        }
        std::error_code ignored;
        std::filesystem::remove_all(dir, ignored);
    }

    std::filesystem::path dir;
    std::filesystem::path messageLog;
    std::filesystem::path dumpFile;
    std::wstring mainClass;
    std::wstring countdownClass;
    std::wstring settingClass;
    std::wstring ignoreClass;
    PROCESS_INFORMATION process{};
    HANDLE job = nullptr;

    HWND Main() const { return FindWindowW(mainClass.c_str(), nullptr); }

    bool WaitForMessage(const std::string& text, DWORD timeoutMs, int minCount = 1) const {
        return WaitForFileCount(messageLog, text, minCount, timeoutMs);
    }

    bool WaitVisible(const std::wstring& cls, DWORD timeoutMs) const {
        return WaitForWindow(cls, true, timeoutMs) != nullptr;
    }

    bool WaitForExit(DWORD timeoutMs, DWORD* exitCode = nullptr) const {
        if (WaitForSingleObject(process.hProcess, timeoutMs) != WAIT_OBJECT_0) {
            return false;
        }
        if (exitCode != nullptr) {
            GetExitCodeProcess(process.hProcess, exitCode);
        }
        return true;
    }

    void Quit() {
        const HWND window = Main();
        if (window != nullptr) {
            PostMessageW(window, wordlock::kTestQuitMessage, 0, 0);
        }
        REQUIRE(WaitForSingleObject(process.hProcess, 1000) == WAIT_OBJECT_0);
    }

private:
    void SetEnv(const std::wstring& name, const std::wstring& value) {
        bool saved = false;
        for (const auto& entry : originalEnv_) {
            if (entry.first == name) {
                saved = true;
                break;
            }
        }
        if (!saved) {
            std::wstring previous;
            const DWORD length = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
            if (length > 0) {
                previous.resize(length);
                GetEnvironmentVariableW(name.c_str(), previous.data(), length);
                previous.resize(length - 1);
            }
            originalEnv_.emplace_back(name, previous);
        }
        REQUIRE(SetEnvironmentVariableW(name.c_str(),
                                        value.empty() ? nullptr : value.c_str()));
    }

    std::vector<std::pair<std::wstring, std::wstring>> originalEnv_;
};

}  // namespace test_support

using test_support::AppExecutablePath;
using test_support::CountOccurrences;
using test_support::ReadFile;
using test_support::TempRoot;
using test_support::TestApp;
using test_support::TruncateFile;
using test_support::WaitForAllMessages;
using test_support::WaitForFileContains;
using test_support::WaitForHiddenWindow;
using test_support::WaitForNonEmptyFile;
using test_support::WaitForWindow;
