#include "freeze_chrome.h"

#include <windows.h>
#include <tlhelp32.h>

#include <cwchar>
#include <mutex>
#include <vector>

namespace {

// NtSuspendProcess / NtResumeProcess are exported by ntdll but not declared
// in the public headers, so load them dynamically.
using NtProcessFunction = LONG(NTAPI*)(HANDLE);

NtProcessFunction load_nt_process_function(const char* name) {
    static HMODULE ntdll = nullptr;

    if (!ntdll) {
        ntdll = GetModuleHandleW(L"ntdll.dll");

        if (!ntdll)
            ntdll = LoadLibraryW(L"ntdll.dll");
    }

    if (!ntdll)
        return nullptr;

    return reinterpret_cast<NtProcessFunction>(
        GetProcAddress(ntdll, name)
    );
}

struct SuspendedProcess {
    DWORD pid = 0;
    FILETIME creationTime{};
    bool hasCreationTime = false;
};

std::mutex g_mutex;
std::vector<SuspendedProcess> g_suspended;

bool read_creation_time(HANDLE process, FILETIME& creationTime) {
    FILETIME exitTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};

    return GetProcessTimes(
        process,
        &creationTime,
        &exitTime,
        &kernelTime,
        &userTime
    ) != 0;
}

bool same_process(const SuspendedProcess& tracked, HANDLE process) {
    if (tracked.hasCreationTime) {
        FILETIME creationTime{};

        if (read_creation_time(process, creationTime) &&
            CompareFileTime(&creationTime, &tracked.creationTime) != 0) {
            return false;
        }
    }

    return true;
}

std::vector<DWORD> find_chrome_processes() {
    std::vector<DWORD> pids;

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS, 0
    );

    if (snapshot == INVALID_HANDLE_VALUE)
        return pids;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"chrome.exe") == 0)
                pids.push_back(entry.th32ProcessID);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    return pids;
}

} // namespace

int freeze_chrome() {
    NtProcessFunction suspendProcess =
        load_nt_process_function("NtSuspendProcess");

    if (!suspendProcess)
        return -1;

    std::lock_guard<std::mutex> lock(g_mutex);

    int suspendedCount = 0;

    for (DWORD pid : find_chrome_processes()) {
        HANDLE process = OpenProcess(
            PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            pid
        );

        if (!process)
            continue;

        SuspendedProcess current;
        current.pid = pid;
        current.hasCreationTime =
            read_creation_time(process, current.creationTime);

        // Avoid increasing Chrome's suspend count on repeated freezes.
        bool alreadySuspended = false;

        for (auto it = g_suspended.begin();
             it != g_suspended.end(); ++it) {

            if (it->pid != pid)
                continue;

            if (same_process(*it, process)) {
                alreadySuspended = true;
            }
            else {
                // The PID was reused by a new process.
                g_suspended.erase(it);
            }

            break;
        }

        if (!alreadySuspended && suspendProcess(process) >= 0) {
            g_suspended.push_back(current);
            ++suspendedCount;
        }

        CloseHandle(process);
    }

    return suspendedCount;
}

int resume_chrome() {
    NtProcessFunction resumeProcess =
        load_nt_process_function("NtResumeProcess");

    if (!resumeProcess)
        return -1;

    std::lock_guard<std::mutex> lock(g_mutex);

    int resumedCount = 0;
    std::vector<SuspendedProcess> stillSuspended;

    for (const SuspendedProcess& tracked : g_suspended) {
        HANDLE process = OpenProcess(
            PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            tracked.pid
        );

        if (!process)
            continue; // Process has exited.

        if (!same_process(tracked, process)) {
            // The PID now belongs to a different process; do not touch it.
            CloseHandle(process);
            continue;
        }

        if (resumeProcess(process) >= 0)
            ++resumedCount;
        else
            stillSuspended.push_back(tracked);

        CloseHandle(process);
    }

    g_suspended.swap(stillSuspended);

    return resumedCount;
}

bool is_chrome_frozen() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return !g_suspended.empty();
}
