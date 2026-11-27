#include "app.h"

namespace wordlock {

void PaintCenteredText(HWND hwnd, const wchar_t* text, int fontHeight,
                       const ThemePalette& palette) {
    PAINTSTRUCT paint{};
    HDC hdc = BeginPaint(hwnd, &paint);

    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);

    HBRUSH background = CreateSolidBrush(palette.background);
    FillRect(hdc, &clientRect, background);
    DeleteObject(background);

    const UINT dpi = GetDpiForWindow(hwnd);
    const int physicalFontHeight = (dpi == 0) ? fontHeight : ScaleForDpi(fontHeight, dpi);
    const int weight = palette.bold ? FW_BOLD : FW_NORMAL;
    HFONT font = CreateFontW(-physicalFontHeight, 0, 0, 0, weight,
                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                             palette.font);

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SetTextColor(hdc, palette.text);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text, -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
    EndPaint(hwnd, &paint);
}

}  // namespace wordlock
