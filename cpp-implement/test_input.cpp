// test_input.cpp
//
// Minimal <input /> demo for the UI framework.
//
// Build:
//   build.bat --input test_input.cpp --output build/test_input.exe
//
// The window shows a centered input field. Every character typed into it is
// mirrored into the label below, which demonstrates dynamic re-rendering via
// UIWindow::set_text().

#include "ui/ui_framework.h"

#include <string>

static const wchar_t* TEST_HTML = LR"HTML(
<div class="h-screen bg-gray-100 p-6 flex flex-col justify-center items-center">
    <input
        id="name-input"
        class="bg-white text-black text-lg p-4 rounded-lg w-[360px]"
        placeholder="Type something..." />

    <p id="typed-label" class="text-lg text-gray-600 mt-4">
        You typed: (nothing)
    </p>
</div>
)HTML";

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    UIWindow window(L"Input Test", 600, 560);

    window.load_html(TEST_HTML);

    // Dynamic rendering: update the label on every edit.
    window.bind_input(
        L"name-input",
        [&window](const std::wstring& value) {
            std::wstring label = L"You typed: ";

            label += value.empty() ? L"(nothing)" : value;

            window.set_text(L"typed-label", label);
        }
    );

    window.run();
    return 0;
}
