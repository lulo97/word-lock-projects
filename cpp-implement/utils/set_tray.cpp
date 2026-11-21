#include "set_tray.h"

#include <map>

namespace {

constexpr UINT TRAY_ICON_ID = 1;
constexpr UINT IDM_TRAY_SHOW = 40001;
constexpr UINT IDM_TRAY_EXIT = 40002;

struct TrayState {
    NOTIFYICONDATAW nid{};
    HICON icon = nullptr;
    bool ownsIcon = false;
    TrayClickCallback clickCallback;
};

std::map<HWND, TrayState> g_trays;

void show_window(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd))
        return;

    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    else
        ShowWindow(hwnd, SW_SHOW);

    SetForegroundWindow(hwnd);
}

void show_context_menu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();

    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING, IDM_TRAY_SHOW, L"Open App");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);

    // Required so the menu closes correctly when clicking elsewhere.
    SetForegroundWindow(hwnd);

    UINT command = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        pt.x,
        pt.y,
        0,
        hwnd,
        nullptr
    );

    DestroyMenu(menu);

    if (command == IDM_TRAY_SHOW)
        show_window(hwnd);
    else if (command == IDM_TRAY_EXIT)
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

} // namespace

bool set_tray(
    HWND hwnd,
    HICON icon,
    const std::wstring& tooltip
) {
    if (!hwnd || !IsWindow(hwnd))
        return false;

    // Replace an existing icon for this window.
    remove_tray(hwnd);

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAY_CALLBACK;

    bool ownsIcon = false;
    HICON trayIcon = icon;

    if (!trayIcon) {
        // Prefer the window's own small icon.
        trayIcon = (HICON)SendMessageW(
            hwnd, WM_GETICON, ICON_SMALL, 0
        );
    }

    if (!trayIcon) {
        // Then an icon resource embedded in the executable.
        trayIcon = (HICON)LoadImageW(
            GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(1),
            IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            LR_DEFAULTCOLOR
        );

        ownsIcon = trayIcon != nullptr;
    }

    if (!trayIcon) {
        // Finally the generic system application icon (shared, do not destroy).
        trayIcon = LoadIconW(
            nullptr, MAKEINTRESOURCEW(32512) // IDI_APPLICATION
        );
    }

    if (!trayIcon)
        return false;

    nid.hIcon = trayIcon;

    std::wstring tip = tooltip;

    if (tip.empty()) {
        wchar_t title[128]{};
        GetWindowTextW(hwnd, title, 128);
        tip = title;
    }

    if (tip.empty())
        tip = L"Application";

    lstrcpynW(nid.szTip, tip.c_str(), 128);

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        if (ownsIcon)
            DestroyIcon(trayIcon);

        return false;
    }

    // Windows 10/11 behaviour: NIN_SELECT / WM_CONTEXTMENU notifications.
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);

    TrayState state;
    state.nid = nid;
    state.icon = trayIcon;
    state.ownsIcon = ownsIcon;

    g_trays[hwnd] = state;

    return true;
}

void set_tray_click_callback(HWND hwnd, TrayClickCallback callback) {
    auto it = g_trays.find(hwnd);

    if (it != g_trays.end())
        it->second.clickCallback = std::move(callback);
}

void remove_tray(HWND hwnd) {
    auto it = g_trays.find(hwnd);

    if (it == g_trays.end())
        return;

    Shell_NotifyIconW(NIM_DELETE, &it->second.nid);

    if (it->second.ownsIcon && it->second.icon)
        DestroyIcon(it->second.icon);

    g_trays.erase(it);
}

void handle_tray_message(HWND hwnd, LPARAM lParam) {
    auto found = g_trays.find(hwnd);

    if (found != g_trays.end() && found->second.clickCallback) {
        found->second.clickCallback(hwnd, lParam);
        return;
    }

    // With NOTIFYICON_VERSION_4, the low word is the notification event.
    // For older versions lParam is the raw mouse message.
    UINT event = LOWORD(lParam);

    switch (event) {
    // Left click (and keyboard select) opens the menu.
    case NIN_SELECT:
    case NIN_KEYSELECT:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:

    // Right click also opens the same menu.
    case WM_CONTEXTMENU:
    case WM_RBUTTONUP:
        show_context_menu(hwnd);
        break;

    default:
        break;
    }
}
