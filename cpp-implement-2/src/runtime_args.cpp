#include "app.h"

namespace wordlock {

namespace {

bool HasTestArg(const wchar_t* wanted) {
    if (!IsTestModeEnabled()) {
        return false;
    }
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return false;
    }
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], wanted) == 0) {
            found = true;
        }
    }
    LocalFree(argv);
    return found;
}

}  // namespace

bool WantsTestSettingsOpen() {
    return HasTestArg(kTestOpenSettingsArg);
}

bool WantsTestDumbbellOpen() {
    return HasTestArg(kTestOpenDumbbellArg);
}

bool WantsTestEnglishOpen() {
    return HasTestArg(kTestOpenEnglishArg);
}

bool WantsTestJapaneseOpen() {
    return HasTestArg(kTestOpenJapaneseArg);
}

}  // namespace wordlock
