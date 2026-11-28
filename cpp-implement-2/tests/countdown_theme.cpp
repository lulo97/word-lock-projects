#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

bool NearChannel(int left, int right, int tolerance = 12) {
    const int delta = left > right ? left - right : right - left;
    return delta <= tolerance;
}

}  // namespace

TEST_CASE("Countdown repaints with the active theme and keeps layered dragging",
          "[countdown][theme][runtime]") {
    REQUIRE(std::filesystem::exists(AppExecutablePath()));

    TestApp app({L"--test-open-settings", {},
                 "window.app.send('theme:preview', { value: 'midnight' });", true});

    HWND countdown = WaitForWindow(app.countdownClass, true, 4000);
    REQUIRE(countdown != nullptr);

    REQUIRE(WaitForNonEmptyFile(app.dumpFile, 5000));
    REQUIRE(app.WaitForMessage("theme:preview", 4000));
    INFO("Host messages: " << ReadFile(app.messageLog));
    REQUIRE(app.WaitForMessage("countdown:theme:midnight", 3000));
    INFO("Host messages after preview: " << ReadFile(app.messageLog));

    const LONG_PTR exStyle = GetWindowLongPtrW(countdown, GWL_EXSTYLE);
    REQUIRE((exStyle & WS_EX_LAYERED) != 0);
    BYTE alpha = 0;
    DWORD flags = 0;
    REQUIRE(GetLayeredWindowAttributes(countdown, nullptr, &alpha, &flags));
    REQUIRE((flags & LWA_ALPHA) != 0);
    REQUIRE(alpha == 128);

    REQUIRE(SendMessageW(countdown, WM_NCHITTEST, 0, MAKELPARAM(20, 20)) == HTCAPTION);

    HDC hdc = GetDC(countdown);
    REQUIRE(hdc != nullptr);
    const COLORREF background = GetPixel(hdc, 6, 6);
    ReleaseDC(countdown, hdc);
    INFO("Countdown background COLORREF: " << background);
    REQUIRE(NearChannel(GetRValue(background), 0x1e));
    REQUIRE(NearChannel(GetGValue(background), 0x29));
    REQUIRE(NearChannel(GetBValue(background), 0x3b));

    REQUIRE(app.Main() != nullptr);
    app.Quit();
}
