#include "app.h"

namespace wordlock {

bool RegisterWindowClass(HINSTANCE hInstance, const wchar_t* className,
                         WNDPROC procedure, const wchar_t* errorText) {
    WNDCLASSW windowClass{};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = procedure;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = LoadAppIcon();
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    windowClass.lpszClassName = className;
    if (!RegisterClassW(&windowClass)) {
        MessageBoxW(nullptr, errorText, L"Error", MB_ICONERROR | MB_OK);
        return false;
    }
    return true;
}

bool RegisterAllWindowClasses(HINSTANCE hInstance, const WindowClasses& classes) {
    if (!RegisterWindowClass(hInstance, classes.main.c_str(), WndProc,
                             L"RegisterClassW failed.")) {
        return false;
    }
    if (!RegisterWindowClass(hInstance, classes.countDown.c_str(), CountDownWndProc,
                             L"RegisterClassW for CountDownWindow failed.")) {
        return false;
    }
    if (!RegisterWindowClass(hInstance, classes.setting.c_str(), SettingWndProc,
                             L"RegisterClassW for SettingWindows failed.")) {
        return false;
    }
    return RegisterWindowClass(hInstance, classes.ignore.c_str(), IgnoreWordsWndProc,
                               L"RegisterClassW for IgnoreWordsWindows failed.");
}

}  // namespace wordlock
