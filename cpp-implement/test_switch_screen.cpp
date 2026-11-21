// test_switch_screen.cpp
//
// Build:
//   build.bat --input test_switch_screen.cpp --output build/test_switch_screen.exe
//
// How screen switching works in this framework:
//
//   UIWindow::load_html(html)
//     -> parse_html(html) stores a new document tree
//     -> InvalidateRect(...) triggers WM_PAINT
//     -> the next paint re-runs layout and draws the new tree
//
// Click bindings are stored in UIWindow::click_events keyed by element id,
// and load_html() does not clear that map. So one bind_click("btn-change")
// keeps working on every screen that uses the same button id.

#include "ui/ui_framework.h"

// Screen 1: one label + one button.
static const wchar_t* SCREEN_1 = LR"HTML(
<div class="bg-gray-100 p-6">
    <p class="text-2xl text-black mb-4">screen1</p>

    <div class="flex justify-center">
        <button id="btn-change"
                class="bg-blue-600 text-white text-base p-4 rounded-lg">
            Change screen
        </button>
    </div>
</div>
)HTML";

// Screen 2: two labels + the same change-screen button.
static const wchar_t* SCREEN_2 = LR"HTML(
<div class="bg-gray-100 p-6">
    <p class="text-2xl text-black mb-2">screen2</p>
    <p class="text-2xl text-black mb-4">screen2</p>

    <div class="flex justify-center">
        <button id="btn-change"
                class="bg-blue-600 text-white text-base p-4 rounded-lg">
            Change screen
        </button>
    </div>
</div>
)HTML";

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    UIWindow window(L"Switch Screen Test", 600, 420);

    bool showingScreen1 = true;
    window.load_html(SCREEN_1);

    // Bound once; survives load_html() because it is keyed by id.
    window.bind_click(
        L"btn-change",
        [&window, &showingScreen1] {
            showingScreen1 = !showingScreen1;

            window.load_html(
                showingScreen1 ? SCREEN_1 : SCREEN_2
            );
        }
    );

    window.run();
    return 0;
}
