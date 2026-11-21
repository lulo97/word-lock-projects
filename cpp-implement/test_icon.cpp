// test_icon.cpp
//
// Demo for utils/set_icon.h.
//
// Build:
//   build.bat --input test_icon.cpp --output build\test_icon.exe
//
// The window icon is applied automatically by UIWindow. Use the buttons to
// re-apply the icon from assets\icon.ico, apply the icon embedded in the
// executable, and check that load_icon() can load the .ico file.

#include "ui/ui_framework.h"
#include "utils/set_icon.h"

#include <string>

static const wchar_t* ICON_HTML = LR"HTML(
<div class="bg-gray-100 p-6">
    <p class="text-2xl text-black mb-2">Icon Test</p>

    <p class="text-sm text-gray-600 mb-4">
        Check the title bar, taskbar and Alt+Tab icon. The icon is also
        embedded in the .exe, so it still works when the executable is
        moved away from the source tree.
    </p>

    <p id="status" class="text-base text-black mb-4">
        Icon: ready
    </p>

    <div class="flex gap-2">
        <button id="btn-file"
                class="bg-blue-600 text-white text-base p-4 rounded-lg">
            Apply assets\icon.ico
        </button>

        <button id="btn-resource"
                class="bg-blue-600 text-white text-base p-4 rounded-lg">
            Apply embedded icon
        </button>

        <button id="btn-load"
                class="bg-green-600 text-white text-base p-4 rounded-lg">
            Test load_icon
        </button>
    </div>
</div>
)HTML";

static HWND test_window() {
    return FindWindowW(L"UIFrameworkWindow", L"Icon Test");
}

static void show_status(UIWindow& window, const std::wstring& text) {
    window.set_text(L"status", text);
}

static void apply_file_icon(UIWindow& window, HWND hwnd) {
    const bool applied = set_icon(hwnd, L"assets\\icon.ico");

    show_status(
        window,
        applied
            ? L"Icon: applied from assets\\icon.ico"
            : L"Icon: assets\\icon.ico was not found"
    );
}

static void apply_embedded_icon(UIWindow& window, HWND hwnd) {
    const bool applied = set_icon_from_resource(hwnd, 1);

    show_status(
        window,
        applied
            ? L"Icon: applied from the executable resource"
            : L"Icon: executable resource ID 1 was not found"
    );
}

static void test_load_icon(UIWindow& window) {
    HICON icon = load_icon(L"assets\\icon.ico", 32, 32);

    if (!icon) {
        show_status(window, L"load_icon: failed to load assets\\icon.ico");
        return;
    }

    std::wstring status = L"load_icon: loaded assets\\icon.ico at 32 px";

    DestroyIcon(icon);

    show_status(window, status);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    UIWindow window(L"Icon Test", 680, 440);

    window.load_html(ICON_HTML);

    HWND hwnd = test_window();

    // UIWindow already applies the icon in its constructor; apply it again
    // to exercise the public set_icon() entry point.
    apply_file_icon(window, hwnd);

    window.bind_click(L"btn-file", [&window, hwnd] {
        apply_file_icon(window, hwnd);
    });

    window.bind_click(L"btn-resource", [&window, hwnd] {
        apply_embedded_icon(window, hwnd);
    });

    window.bind_click(L"btn-load", [&window] {
        test_load_icon(window);
    });

    window.run();
    return 0;
}
