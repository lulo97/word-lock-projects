#include "app.h"

namespace wordlock {

void RememberCountDownPosition(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }
    RECT rect{};
    if (GetWindowRect(hwnd, &rect)) {
        g_countDownPosition.x = rect.left;
        g_countDownPosition.y = rect.top;
        g_countDownPositionValid = true;
    }
}

void ApplyCountDownPosition(HWND hwnd) {
    if (hwnd == nullptr || !g_countDownPositionValid) {
        return;
    }
    SetWindowPos(hwnd, nullptr, g_countDownPosition.x, g_countDownPosition.y,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace wordlock
