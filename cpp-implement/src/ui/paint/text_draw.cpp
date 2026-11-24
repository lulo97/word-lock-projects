#include "paint/text_draw.h"

#include "core/utf.h"
#include "paint/canvas.h"
#include "paint/font_cache.h"

#include <algorithm>
#include <cwctype>

namespace ui {

namespace {

bool hasCjk(const std::wstring& text) {
    for (wchar_t c : text) {
        if (isCjk(c))
            return true;
    }

    return false;
}

HFONT fontForRun(const TextRun& run) {
    FontKey key;
    key.family = (run.forceCjk || hasCjk(run.text)) ? L"Yu Gothic UI" : run.family;
    key.px = std::max(1, static_cast<int>(run.fontPx + 0.5f));
    key.weight = run.weight;
    key.italic = run.italic;
    return getFont(key);
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

    run.family = style.fontFamily;
    run.fontPx = std::max(1.f, style.fontSize * scale);
    run.weight = style.fontWeight;
    run.italic = style.italic;
    run.letterSpacing = style.letterSpacing * scale;
    run.color = style.color;
    run.lineHeightPx = style.lineHeightIsPx
        ? style.lineHeight * scale
        : style.fontSize * scale * style.lineHeight;
    return run;
}

void measureRun(TextRun& run) {
    HDC hdc = GetDC(nullptr);

    if (!hdc) {
        run.width = static_cast<float>(run.text.size()) * run.fontPx * 0.55f;
        run.height = run.fontPx * 1.2f;
        return;
    }

    HFONT font = fontForRun(run);
    HGDIOBJ old = SelectObject(hdc, font);

    SIZE size{};
    GetTextExtentPoint32W(
        hdc,
        run.text.c_str(),
        static_cast<int>(run.text.size()),
        &size
    );

    TEXTMETRICW tm{};
    GetTextMetricsW(hdc, &tm);

    run.width = static_cast<float>(size.cx) +
                run.letterSpacing * std::max<int>(0, static_cast<int>(run.text.size()) - 1);
    run.height = static_cast<float>(tm.tmHeight);

    SelectObject(hdc, old);
    ReleaseDC(nullptr, hdc);
}

void drawRun(Canvas& canvas, const TextRun& run) {
    if (run.text.empty())
        return;

    HDC hdc = canvas.hdc();

    if (!hdc)
        return;

    HFONT font = fontForRun(run);
    HGDIOBJ oldFont = SelectObject(hdc, font);
    int oldMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, run.color.toColorRef());

    if (run.letterSpacing != 0) {
        float x = run.x;

        for (wchar_t c : run.text) {
            TextOutW(hdc, static_cast<int>(x + 0.5f), static_cast<int>(run.y), &c, 1);

            SIZE size{};
            GetTextExtentPoint32W(hdc, &c, 1, &size);
            x += size.cx + run.letterSpacing;
        }
    }
    else {
        TextOutW(
            hdc,
            static_cast<int>(run.x),
            static_cast<int>(run.y),
            run.text.c_str(),
            static_cast<int>(run.text.size())
        );
    }

    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldMode);
    SelectObject(hdc, oldFont);
}

std::vector<float> charOffsets(const TextRun& run) {
    std::vector<float> offsets(run.text.size() + 1, 0.f);

    HDC hdc = GetDC(nullptr);

    if (!hdc) {
        for (size_t i = 0; i <= run.text.size(); ++i)
            offsets[i] = static_cast<float>(i) * run.fontPx * 0.55f;

        return offsets;
    }

    HFONT font = fontForRun(run);
    HGDIOBJ old = SelectObject(hdc, font);

    for (size_t i = 1; i <= run.text.size(); ++i) {
        SIZE size{};
        GetTextExtentPoint32W(hdc, run.text.c_str(), static_cast<int>(i), &size);
        offsets[i] = static_cast<float>(size.cx) +
                     run.letterSpacing * static_cast<float>(i - 1);
    }

    SelectObject(hdc, old);
    ReleaseDC(nullptr, hdc);

    return offsets;
}

} // namespace ui
