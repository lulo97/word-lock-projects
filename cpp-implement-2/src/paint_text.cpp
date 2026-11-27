#include "app.h"

namespace wordlock {

void PaintCenteredText(HWND hwnd, const wchar_t* text, int fontHeight) {
    PAINTSTRUCT paint{};
    HDC hdc = BeginPaint(hwnd, &paint);

    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);

    // Pure white background.
    FillRect(hdc, &clientRect, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    const UINT dpi = GetDpiForWindow(hwnd);
    const int physicalFontHeight = (dpi == 0)
                                       ? fontHeight
                                       : ScaleForDpi(fontHeight, dpi);

    HFONT font = CreateFontW(
        -physicalFontHeight,              // Height in physical pixels (negative = character height)
        0,                                // Default width
        0, 0,                             // Default escapement/orientation
        FW_NORMAL,                        // Weight
        FALSE, FALSE, FALSE,              // No italic, underline, or strikeout
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);

    DrawTextW(hdc, text, -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);

    EndPaint(hwnd, &paint);
}

}  // namespace wordlock

