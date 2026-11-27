#include "app.h"

namespace wordlock {

bool WantsTestSettingsOpen() {
    if (!IsTestModeEnabled()) {
        return false;
    }
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return false;
    }
    bool open = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], kTestOpenSettingsArg) == 0) {
            open = true;
        }
    }
    LocalFree(argv);
    return open;
}

bool WantsTestDumbbellOpen() {
    if (!IsTestModeEnabled()) {
        return false;
    }
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return false;
    }
    bool open = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], kTestOpenDumbbellArg) == 0) {
            open = true;
        }
    }
    LocalFree(argv);
    return open;
}

}  // namespace wordlock
