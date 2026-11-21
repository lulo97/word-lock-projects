// test_tray.cpp
//
// Demo for utils/set_tray.h.
//
// Build:
//   build.bat --input test_tray.cpp --output build/test_tray.exe
//
// The app adds itself to the Windows notification area on startup. Use the
// buttons to add/remove the icon at runtime. Left-click or right-click the
// tray icon for the Open App / Exit menu.

#include "ui/ui_framework.h"

#include <string>

static const wchar_t* TRAY_HTML = LR"HTML(
<div class="bg-gray-100 p-6">
    <p class="text-2xl text-black mb-2">Tray Test</p>

    <p class="text-sm text-gray-600 mb-4">
        Left-click or right-click the tray icon for the
        Open App / Exit menu.
    </p>

    <p id="status" class="text-base text-black mb-4">
        Tray icon: starting...
    </p>

    <div class="flex gap-2">
        <button id="btn-add"
                class="bg-blue-600 text-white text-base p-4 rounded-lg">
            Add tray icon
        </button>

        <button id="btn-remove"
                class="bg-red-500 text-white text-base p-4 rounded-lg">
            Remove tray icon
        </button>
    </div>
</div>
)HTML";

static void show_status(UIWindow& window, bool active) {
    window.set_text(
        L"status",
        active
            ? L"Tray icon: active"
            : L"Tray icon: not present"
    );
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    UIWindow window(L"Tray Test", 620, 420);

    window.load_html(TRAY_HTML);

    bool trayActive = window.set_tray();
    show_status(window, trayActive);

    window.bind_click(L"btn-add", [&window, &trayActive] {
        trayActive = window.set_tray();
        show_status(window, trayActive);
    });

    window.bind_click(L"btn-remove", [&window, &trayActive] {
        window.remove_tray();
        trayActive = false;
        show_status(window, trayActive);
    });

    window.run();
    return 0;
}
