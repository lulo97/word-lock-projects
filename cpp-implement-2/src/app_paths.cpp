#include "app.h"

namespace wordlock {

std::wstring GetExecutableDirectory() {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring path(exePath);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash + 1);
    }
    return path;
}

std::wstring AppAssetUrl(const wchar_t* file) {
    return std::wstring(L"http://app/") + file;
}

bool IsTestDataMode() {
    return EnvFlagEnabled(kUseTestDataEnv);
}

std::wstring GetDataDirectory() {
    if (IsTestDataMode()) {
        return std::wstring();
    }

    if (IsTestModeEnabled()) {
        std::wstring directory = GetEnvironmentValue(kTestHomeEnv);
        if (directory.empty()) {
            return std::wstring();
        }
        if (directory.back() != L'\\') {
            directory.push_back(L'\\');
        }
        std::filesystem::create_directories(directory);
        return directory;
    }

    wchar_t home[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", home, static_cast<DWORD>(MAX_PATH));
    std::wstring directory = length > 0 ? std::wstring(home) : std::wstring(L"C:\\");
    if (!directory.empty() && directory.back() != L'\\') {
        directory.push_back(L'\\');
    }
    directory += L"pomodoro_lock_cpp\\";
    std::filesystem::create_directories(directory);
    return directory;
}

}  // namespace wordlock
