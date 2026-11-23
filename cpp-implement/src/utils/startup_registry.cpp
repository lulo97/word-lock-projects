// startup_registry.cpp
//
// HKCU Run key backend for "start with Windows" (M8). See the header for the
// seam used by headless tests.

#include "utils/startup_registry.h"

#include <windows.h>

namespace {

constexpr const wchar_t* kRunKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kRunValue = L"KotobaCpp";

} // namespace

std::wstring currentExePath() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
        if (n == 0)
            return {};
        if (n < path.size()) {
            path.resize(n);
            return path;
        }
        // Truncated (long path): grow and retry.
        path.resize(path.size() * 2);
    }
}

std::wstring startupCommandLine(const std::wstring& exePath) {
    return L"\"" + exePath + L"\" --minimized";
}

std::wstring WinStartupRegistry::command() const {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS)
        return {};

    DWORD type = 0;
    DWORD bytes = 0;
    LONG rc = RegQueryValueExW(key, kRunValue, nullptr, &type, nullptr, &bytes);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) ||
        bytes < sizeof(wchar_t)) {
        RegCloseKey(key);
        return {};
    }

    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    rc = RegQueryValueExW(key, kRunValue, nullptr, &type,
                          reinterpret_cast<BYTE*>(value.data()), &bytes);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS)
        return {};

    // Drop the trailing NUL the registry includes in the byte count.
    while (!value.empty() && value.back() == L'\0')
        value.pop_back();
    return value;
}

bool WinStartupRegistry::isEnabled() const {
    return !command().empty();
}

bool WinStartupRegistry::setEnabled(bool enabled, const std::wstring& command) {
    if (!enabled) {
        HKEY key = nullptr;
        LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0,
                                KEY_SET_VALUE, &key);
        if (rc == ERROR_FILE_NOT_FOUND)
            return true;   // nothing to delete
        if (rc != ERROR_SUCCESS)
            return false;
        rc = RegDeleteValueW(key, kRunValue);
        RegCloseKey(key);
        return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
    }

    HKEY key = nullptr;
    DWORD disposition = 0;
    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                              KEY_SET_VALUE, nullptr, &key, &disposition);
    if (rc != ERROR_SUCCESS)
        return false;
    rc = RegSetValueExW(
        key, kRunValue, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()),
        (DWORD)((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

bool FakeStartupRegistry::setEnabled(bool enabled, const std::wstring& command) {
    enabled_ = enabled;
    command_ = enabled ? command : std::wstring();
    return true;
}
