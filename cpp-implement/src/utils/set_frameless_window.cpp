#include "set_frameless_window.h"

void set_frameless_window(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd))
        return;

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);

    // Remove everything that draws or provides system window chrome.
    style &= ~(
        WS_CAPTION |
        WS_SYSMENU |
        WS_THICKFRAME |
        WS_MINIMIZEBOX |
        WS_MAXIMIZEBOX |
        WS_BORDER |
        WS_DLGFRAME
    );

    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    // Remove any extended 3D/edge borders.
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    exStyle &= ~(
        WS_EX_DLGMODALFRAME |
        WS_EX_WINDOWEDGE |
        WS_EX_CLIENTEDGE |
        WS_EX_STATICEDGE
    );

    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

    // Apply the new frame without moving or resizing the window.
    SetWindowPos(
        hwnd,
        nullptr,
        0, 0, 0, 0,
        SWP_NOMOVE |
        SWP_NOSIZE |
        SWP_NOZORDER |
        SWP_NOACTIVATE |
        SWP_FRAMECHANGED
    );
}

void set_framed_window(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd))
        return;

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);

    style &= ~WS_POPUP;
    style |= WS_OVERLAPPEDWINDOW;

    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    SetWindowPos(
        hwnd,
        nullptr,
        0, 0, 0, 0,
        SWP_NOMOVE |
        SWP_NOSIZE |
        SWP_NOZORDER |
        SWP_NOACTIVATE |
        SWP_FRAMECHANGED
    );
}
