#pragma once

#include <windows.h>

// Freeze/resume all running chrome.exe processes on Windows.
//
// freeze_chrome() suspends every accessible chrome.exe process (browser and
// its child processes). Calling it again while already frozen is a no-op for
// processes this module already suspended.
//
// Returns the number of processes suspended/resumed, or -1 if the NT process
// suspend/resume API is unavailable.
int freeze_chrome();
int resume_chrome();

// True while processes suspended by freeze_chrome() are still tracked.
bool is_chrome_frozen();

// Startup recovery (M9): resume EVERY chrome.exe, not just the pids this
// process tracked. resume_chrome() alone cannot thaw Chrome after a crash
// or a Task Manager kill, because the new process starts with an empty
// tracking list. Resuming a non-suspended process is a safe no-op.
// Returns the number of processes resumed, or -1 if the NT API is missing.
int resume_all_chrome();
