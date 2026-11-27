#include "app.h"

namespace wordlock {

int ScaleForDpi(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

void HandleDpiChanged(HWND hwnd, LPARAM lParam) {
    const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(hwnd, nullptr,
                 suggested->left, suggested->top,
                 suggested->right - suggested->left,
                 suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd, nullptr, TRUE);
}

bool EnvFlagEnabled(const wchar_t* name) {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
    return length > 0 && length < std::size(value) && wcscmp(value, L"1") == 0;
}

bool IsTestModeEnabled() {
    return EnvFlagEnabled(kTestModeEnv);
}

std::wstring GetEnvironmentValue(const wchar_t* name) {
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (length == 0) {
        return std::wstring();
    }
    std::vector<wchar_t> buffer(length, L'\0');
    GetEnvironmentVariableW(name, buffer.data(), length);
    return std::wstring(buffer.data());
}

std::wstring WindowClassForCurrentProcess(const wchar_t* base) {
    if (!IsTestModeEnabled()) {
        return std::wstring(base);
    }
    return std::wstring(base) + L"Test" + std::to_wstring(GetCurrentProcessId());
}

bool IsChromeFreezeMockEnabled() {
    return EnvFlagEnabled(kChromeFreezeMockEnv);
}

bool AcquireSingleInstanceLock() {
    if (IsTestModeEnabled() || IsChromeFreezeMockEnabled()) {
        return true;
    }

    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
    if (g_singleInstanceMutex == nullptr) {
        return true;  // Fail open: a lock error must not block the real instance.
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = nullptr;
        return false;
    }

    return true;
}

void ResumeHeldChromeProcesses() {
    ResumeAllChromeProcesses();
}

}  // namespace wordlock
