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

#include "kotoba_app.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR cmd, int) {
    KotobaCppApp app;
    if (cmd && wcsstr(cmd, L"--debug"))
        app.setDebug(true);
    return app.run();
}
