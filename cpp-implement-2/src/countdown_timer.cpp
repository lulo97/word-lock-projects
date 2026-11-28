#include "app.h"

namespace wordlock {

// The tray and the settings window stop the countdown independently. The timer
// runs only when neither of them has stopped it.
void SyncCountdownTimer() {
    if (!g_countDownWindow || !IsWindow(g_countDownWindow)) {
        return;
    }
    KillTimer(g_countDownWindow, kCountDownTimerId);
    if (!g_countdownPaused && !g_countdownSettingsPaused) {
        SetTimer(g_countDownWindow, kCountDownTimerId,
                 TestIntervalMs(kTestCountdownTickMsEnv, 1000), nullptr);
    }
}

}  // namespace wordlock
