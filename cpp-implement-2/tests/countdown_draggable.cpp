#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

bool Near(int left, int right, int tolerance) {
    const int delta = left > right ? left - right : right - left;
    return delta <= tolerance;
}

void DragWindow(HWND hwnd, int deltaX, int deltaY) {
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    RECT rect{};
    REQUIRE(GetWindowRect(hwnd, &rect));
    const int startX = rect.left + 20;
    const int startY = rect.top + 20;
    REQUIRE(SetCursorPos(startX, startY));

    INPUT down{};
    down.type = INPUT_MOUSE;
    down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    REQUIRE(SendInput(1, &down, sizeof(INPUT)) == 1);

    for (int step = 1; step <= 5; ++step) {
        REQUIRE(SetCursorPos(startX + deltaX * step / 5, startY + deltaY * step / 5));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    INPUT up{};
    up.type = INPUT_MOUSE;
    up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    REQUIRE(SendInput(1, &up, sizeof(INPUT)) == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

}  // namespace

TEST_CASE("Countdown drags with a move cursor and remembers position until restart",
          "[countdown][drag][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp first({});
    HWND firstCountDown = WaitForWindow(first.countdownClass, true, 3000);
    REQUIRE(firstCountDown != nullptr);
    REQUIRE(WaitForWindow(first.mainClass, false, 2000) != nullptr);

    RECT initial{};
    REQUIRE(GetWindowRect(firstCountDown, &initial));
    const int hitX = initial.left + 20;
    const int hitY = initial.top + 20;
    REQUIRE(SendMessageW(firstCountDown, WM_NCHITTEST, 0, MAKELPARAM(hitX, hitY)) == HTCAPTION);
    SendMessageW(firstCountDown, WM_SETCURSOR, 0, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    INFO("Countdown host messages: " << ReadFile(first.messageLog));
    REQUIRE(first.WaitForMessage("countdown:cursor-sizeall", 2000));

    constexpr int deltaX = -150;
    constexpr int deltaY = -100;
    DragWindow(firstCountDown, deltaX, deltaY);

    RECT dragged{};
    REQUIRE(GetWindowRect(firstCountDown, &dragged));
    INFO("Initial countdown rect: " << initial.left << "," << initial.top);
    INFO("Dragged countdown rect: " << dragged.left << "," << dragged.top);
    REQUIRE(Near(dragged.left, initial.left + deltaX, 12));
    REQUIRE(Near(dragged.top, initial.top + deltaY, 12));

    HWND firstMain = first.Main();
    REQUIRE(firstMain != nullptr);
    REQUIRE(SendMessageW(firstMain, WM_CLOSE, 0, 0) == 0);
    REQUIRE(WaitForHiddenWindow(firstMain, 1500));

    RECT afterRestart{};
    REQUIRE(GetWindowRect(firstCountDown, &afterRestart));
    REQUIRE(Near(afterRestart.left, dragged.left, 4));
    REQUIRE(Near(afterRestart.top, dragged.top, 4));
    first.Quit();

    // A fresh instance starts from the default position, not the dragged one.
    TestApp second({});
    HWND secondCountDown = WaitForWindow(second.countdownClass, true, 3000);
    REQUIRE(secondCountDown != nullptr);
    RECT fresh{};
    REQUIRE(GetWindowRect(secondCountDown, &fresh));
    INFO("Fresh launch rect: " << fresh.left << "," << fresh.top);
    REQUIRE(Near(fresh.left, initial.left, 4));
    REQUIRE(Near(fresh.top, initial.top, 4));
    second.Quit();
}
