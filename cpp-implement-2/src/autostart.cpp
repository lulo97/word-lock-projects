#include "app.h"

namespace wordlock {

std::wstring GetLaunchExecutablePath() {
    wchar_t portable[MAX_PATH]{};
    const DWORD portableLength = GetEnvironmentVariableW(
        kPortableExecutableEnv, portable, static_cast<DWORD>(std::size(portable)));
    if (portableLength > 0 && portableLength < std::size(portable)) {
        return std::wstring(portable);
    }

    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    return std::wstring(executable);
}

bool IsAutoStartDisabledByWindows(const std::wstring& valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
            0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    BYTE data[32]{};
    DWORD size = sizeof(data);
    DWORD type = 0;
    const LSTATUS result = RegQueryValueExW(key, valueName.c_str(), nullptr, &type, data, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_BINARY && size > 0 && data[0] == 0x03;
}

void RemoveLegacyAutoStartEntry(HKEY key, const std::wstring& launchPath) {
    constexpr wchar_t kLegacyRunValue[] = L"PomodoroLock";

    wchar_t raw[2048]{};
    DWORD size = sizeof(raw);
    DWORD type = 0;
    if (RegQueryValueExW(key, kLegacyRunValue, nullptr, &type,
                         reinterpret_cast<BYTE*>(raw), &size) != ERROR_SUCCESS ||
        type != REG_SZ) {
        return;
    }

    std::wstring command(raw);
    if (!command.empty() && command.front() == L'"') {
        const size_t quote = command.find(L'"', 1);
        if (quote != std::wstring::npos) {
            command = command.substr(1, quote - 1);
        }
    }
    if (_wcsicmp(command.c_str(), launchPath.c_str()) == 0) {
        RegDeleteValueW(key, kLegacyRunValue);
    }
}

void ApplyAutoStartPreference(bool enabled) {
    if (IsTestModeEnabled()) {
        return;
    }

    constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t kRunValue[] = L"PomodoroLockCpp";

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return;
    }

    RemoveLegacyAutoStartEntry(key, GetLaunchExecutablePath());

    if (IsAutoStartDisabledByWindows(kRunValue)) {
        RegCloseKey(key);
        return;
    }

    if (enabled) {
        const std::wstring command = L"\"" + GetLaunchExecutablePath() + L"\"";
        RegSetValueExW(key, kRunValue, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(command.c_str()),
                       static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kRunValue);
    }
    RegCloseKey(key);
}

void SyncAutoStartOnStartup() {
    ApplyAutoStartPreference(g_settings.autoStart);
}

}  // namespace wordlock
