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
