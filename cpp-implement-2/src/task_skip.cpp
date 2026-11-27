#include "app.h"

namespace wordlock {

void AdvanceTheme() {
    g_settings.theme = NextThemeId(g_settings.theme);
    SaveSettings();
    BroadcastTheme(g_settings.theme);
}

void SkipCurrentTask() {
    AdvanceTheme();
    ShowMainWindow();
}

}  // namespace wordlock
