#include "app.h"

namespace wordlock {

void ResumeAllChromeProcesses() {
    if (IsTestModeEnabled() || IsChromeFreezeMockEnabled()) {
        g_heldChromeProcesses.clear();
        return;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    const DWORD currentPid = GetCurrentProcessId();
    if (Process32FirstW(snapshot, &process)) {
        do {
            if (process.th32ProcessID != currentPid &&
                _wcsicmp(process.szExeFile, kChromeProcessName) == 0) {
                ChangeChromeProcessState(process.th32ProcessID, false);
            }
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);
    g_heldChromeProcesses.clear();
}

void FreezeChromeProcesses() {
    if (IsTestModeEnabled() || IsChromeFreezeMockEnabled()) {
        return;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    const DWORD currentPid = GetCurrentProcessId();
    if (Process32FirstW(snapshot, &process)) {
        do {
            const DWORD pid = process.th32ProcessID;
            if (pid == currentPid || _wcsicmp(process.szExeFile, kChromeProcessName) != 0) {
                continue;
            }
            FILETIME startTime{};
            if (!GetProcessStartTime(pid, startTime)) {
                continue;
            }
            const auto held = g_heldChromeProcesses.find(pid);
            if (held != g_heldChromeProcesses.end() && SameStartTime(held->second, startTime)) {
                continue;
            }
            if (ChangeChromeProcessState(pid, true)) {
                g_heldChromeProcesses[pid] = startTime;
            }
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);
}

}  // namespace wordlock
