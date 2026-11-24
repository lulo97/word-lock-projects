// app.cpp
//
// Build:
//   build.bat --input app.cpp --output build\app.exe
//
// KotobaCpp: tray app that shows a countdown, then an input screen,
// then an info screen for a randomly picked Japanese word.
//
// This file is only the entry point. The app itself lives in
// kotoba_app.h / kotoba_app.cpp so headless feature tests (app_test)
// can drive KotobaCppApp without a message loop.

#include "utils/freeze_chrome.h"
#include "utils/kotoba_app.h"

// M9 safety net: if the app dies from an unhandled exception, Chrome must
// not be left frozen. The filter runs before the process goes.
static LONG WINAPI onCrash(EXCEPTION_POINTERS*) {
    resume_chrome();
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR cmd, int) {
    // M9 startup recovery: thaw any Chrome a previous (crashed or killed)
    // run left frozen. Safe no-op when nothing is suspended.
    resume_all_chrome();
    SetUnhandledExceptionFilter(onCrash);
    
    // Test/agent seam: thaw Chrome left frozen by a crashed/killed run and exit.
        if (cmd && wcsstr(cmd, L"--unfreeze"))
            return 0;
            
    KotobaCppApp app;
    if (cmd && wcsstr(cmd, L"--debug"))
        app.setDebug(true);
    // Written by the "start with Windows" Run entry (M8): launch in the tray.
    if (cmd && wcsstr(cmd, L"--minimized"))
        app.setMinimized(true);
    return app.run();
}
