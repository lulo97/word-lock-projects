#include "app.h"

namespace wordlock {

NtProcessFn GetNtProcessFunction(const char* name) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<NtProcessFn>(GetProcAddress(ntdll, name));
}

bool SameStartTime(const FILETIME& left, const FILETIME& right) {
    return left.dwLowDateTime == right.dwLowDateTime &&
           left.dwHighDateTime == right.dwHighDateTime;
}

bool GetProcessStartTime(DWORD processId, FILETIME& startTime) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return false;
    }
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    const BOOL ok = GetProcessTimes(process, &creation, &exit, &kernel, &user);
    CloseHandle(process);
    if (!ok) {
        return false;
    }
    startTime = creation;
    return true;
}

bool IsChromeProcessId(DWORD processId) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    bool found = false;
    if (Process32FirstW(snapshot, &process)) {
        do {
            if (process.th32ProcessID == processId) {
                found = _wcsicmp(process.szExeFile, kChromeProcessName) == 0;
                break;
            }
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);
    return found;
}

bool ChangeChromeProcessState(DWORD processId, bool suspend) {
    NtProcessFn function = GetNtProcessFunction(suspend ? "NtSuspendProcess" : "NtResumeProcess");
    if (function == nullptr) {
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, processId);
    if (process == nullptr) {
        return false;
    }
    const LONG status = function(process);
    CloseHandle(process);
    return status >= 0;
}

}  // namespace wordlock
