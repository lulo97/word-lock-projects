#include "paint/text_draw.h"

#include "core/utf.h"
#include "paint/canvas.h"
#include "paint/font_cache.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <vector>

namespace ui {

namespace {

bool hasCjk(const std::wstring& text) {
    for (wchar_t c : text) {
        if (isCjk(c))
            return true;
    }
    return false;
}

FontKey keyFor(const TextRun& run) {
    FontKey k;
    k.family = run.family.empty() ? L"Segoe UI" : run.family;
    k.px = std::max(1, static_cast<int>(std::lround(run.fontPx)));
    k.weight = run.weight;
    k.italic = run.italic;
    return k;
}

} // namespace

TextRun makeRun(const std::wstring& input, const ComputedStyle& style, float scale) {
    TextRun run;
    run.text = input;

    if (style.transform == TextTransform::Uppercase) {
        for (wchar_t& c : run.text)
            c = static_cast<wchar_t>(towupper(c));
    }
    else if (style.transform == TextTransform::Lowercase) {
        for (wchar_t& c : run.text)
            c = static_cast<wchar_t>(towlower(c));
    }

    // Family decided once from the whole text, so tokens/sub-runs never pick a
    // different font than measuring/drawing the line.
    run.family = hasCjk(run.text) ? L"Yu Gothic UI" : style.fontFamily;

    const int px = std::max(1, static_cast<int>(std::lround(style.fontSize * scale)));
    run.fontPx = static_cast<float>(px); // snapped once
    run.weight = style.fontWeight;
    run.italic = style.italic;
    run.letterSpacing = style.letterSpacing * scale;
    run.color = style.color;
    run.lineHeightPx = style.lineHeightIsPx
        ? style.lineHeight * scale
        : static_cast<float>(px) * style.lineHeight;
    return run;
}

void measureRun(TextRun& run) {
    const FontMetrics& fm = getFontMetrics(keyFor(run));
    float w = 0;
    for (wchar_t c : run.text)
        w += static_cast<float>(fm.advance(c)) + run.letterSpacing;
    run.width = w;
    run.height = static_cast<float>(fm.height);
    run.ascent = static_cast<float>(fm.ascent);
    run.descent = static_cast<float>(fm.descent);
    run.internalLeading = static_cast<float>(fm.internalLeading);
    run.externalLeading = static_cast<float>(fm.externalLeading);
}

TextRun sliceRun(const TextRun& src, const std::wstring& text) {
    TextRun out = src;
    out.text = text;
    out.hanging = false;
    measureRun(out);
    return out;
}

void drawRun(Canvas& canvas, const TextRun& run) {
    if (run.text.empty())
        return;

    HDC hdc = canvas.hdc();
    if (!hdc)
        return;

    const FontMetrics& fm = getFontMetrics(keyFor(run));
    HGDIOBJ oldFont = SelectObject(hdc, fm.font);
    int oldMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, run.color.toColorRef());

    // Hand GDI our advances so glyph positions come from the same numbers layout used.
    std::vector<INT> dx(run.text.size());
    float cum = 0;
    int prev = 0;
    for (size_t i = 0; i < run.text.size(); ++i) {
        cum += static_cast<float>(fm.advance(run.text[i])) + run.letterSpacing;
        int cur = static_cast<int>(std::lround(cum));
        dx[i] = cur - prev;
        prev = cur;
    }

    ExtTextOutW(hdc, snapPx(run.x), snapPx(run.y), 0, nullptr,
                run.text.c_str(), static_cast<UINT>(run.text.size()), dx.data());

    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldMode);
    SelectObject(hdc, oldFont);
}

std::vector<float> charOffsets(const TextRun& run) {
    const FontMetrics& fm = getFontMetrics(keyFor(run));
    std::vector<float> o(run.text.size() + 1, 0.f);
    for (size_t i = 0; i < run.text.size(); ++i)
        o[i + 1] = o[i] + static_cast<float>(fm.advance(run.text[i])) + run.letterSpacing;
    return o;
}

} // namespace ui
