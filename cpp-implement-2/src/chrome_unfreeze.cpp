#include "app.h"

namespace wordlock {

void UnfreezeHeldChromeProcesses() {
    if (IsChromeFreezeMockEnabled()) {
        AppendTestMessageLog(L"chrome:unfreeze");
        g_heldChromeProcesses.clear();
        return;
    }

    const auto held = g_heldChromeProcesses;
    for (const auto& entry : held) {
        FILETIME startTime{};
        if (!IsChromeProcessId(entry.first) || !GetProcessStartTime(entry.first, startTime) ||
            !SameStartTime(startTime, entry.second)) {
            continue;
        }
        ChangeChromeProcessState(entry.first, false);
    }
    g_heldChromeProcesses.clear();
}

void UnfreezeChromeOnExit() {
    if (g_chromeFreezeShutdownDone) {
        return;
    }
    g_chromeFreezeShutdownDone = true;
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        KillTimer(g_mainWindow, kChromeFreezeTimerId);
    }
    UnfreezeHeldChromeProcesses();
}

void SyncChromeFreeze() {
    if (!g_mainWindow || !IsWindow(g_mainWindow)) {
        return;
    }
    if (IsTestModeEnabled() && !IsChromeFreezeMockEnabled()) {
        return;
    }
    const bool shouldFreeze = g_settings.chromeFreeze && IsWindowVisible(g_mainWindow);
    if (shouldFreeze) {
        FreezeChromeProcesses();
        SetTimer(g_mainWindow, kChromeFreezeTimerId, 2000, nullptr);
    } else {
        KillTimer(g_mainWindow, kChromeFreezeTimerId);
        UnfreezeHeldChromeProcesses();
    }
}

}  // namespace wordlock
