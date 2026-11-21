
#include "logic.h"

void paint_buffered(
    HWND hwnd,
    Element* document
) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT client;
    GetClientRect(hwnd, &client);

    int width = client.right;
    int height = client.bottom;

    if (width <= 0 || height <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }

    HDC memDC = CreateCompatibleDC(hdc);

    if (!memDC) {
        FillRect(
            hdc,
            &client,
            (HBRUSH)GetStockObject(WHITE_BRUSH)
        );

        EndPaint(hwnd, &ps);
        return;
    }

    HBITMAP memBitmap = CreateCompatibleBitmap(
        hdc, width, height
    );

    if (!memBitmap) {
        DeleteDC(memDC);

        FillRect(
            hdc,
            &client,
            (HBRUSH)GetStockObject(WHITE_BRUSH)
        );

        EndPaint(hwnd, &ps);
        return;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(
        memDC, memBitmap
    );

    FillRect(
        memDC,
        &client,
        (HBRUSH)GetStockObject(WHITE_BRUSH)
    );

    if (document) {
        RECT area = {
            20, 20,
            width - 20,
            height - 20
        };

        render_html(memDC, *document, area);
    }

    BitBlt(
        hdc,
        0, 0,
        width, height,
        memDC,
        0, 0,
        SRCCOPY
    );

    SelectObject(memDC, oldBitmap);

    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}
