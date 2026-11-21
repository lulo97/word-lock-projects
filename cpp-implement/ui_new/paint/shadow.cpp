#include "paint/shadow.h"

#include "core/utf.h"
#include "style/palette.h"
#include "style/spacing.h"

#include <algorithm>

namespace ui {

void drawShadow(
    Canvas& canvas,
    const RectF& rect,
    const CornerRadii& radii,
    const Shadow& shadow,
    float scale
) {
    if (shadow.color.a == 0)
        return;

    const int layers = 6;
    float blur = std::max(0.f, shadow.blur * scale);
    float spread = shadow.spread * scale;
    float ox = shadow.offsetX * scale;
    float oy = shadow.offsetY * scale;

    for (int i = layers; i >= 1; --i) {
        float grow = spread + blur * static_cast<float>(i) / layers;
        float alpha = shadow.color.a / (layers + 2.0f);

        Color layerColor = shadow.color;
        layerColor.a = static_cast<uint8_t>(
            std::max(0.f, std::min(255.f, alpha))
        );

        RectF layer {
            rect.x + ox - grow,
            rect.y + oy - grow,
            rect.w + grow * 2,
            rect.h + grow * 2
        };

        CornerRadii layerRadii = radii;
        layerRadii.tl = std::max(0.f, layerRadii.tl + grow);
        layerRadii.tr = std::max(0.f, layerRadii.tr + grow);
        layerRadii.br = std::max(0.f, layerRadii.br + grow);
        layerRadii.bl = std::max(0.f, layerRadii.bl + grow);

        canvas.fillRoundRect(layer, layerRadii, layerColor);
    }
}

std::vector<Shadow> shadowPreset(const std::wstring& input) {
    std::wstring name = toLower(input);
    Shadow s;
    s.color = Color::rgba(0, 0, 0, 26);

    if (name == L"sm") {
        s.offsetY = 1;
        s.blur = 2;
        s.color = Color::rgba(0, 0, 0, 13);
    }
    else if (name == L"md" || name.empty()) {
        s.offsetY = 4;
        s.blur = 6;
        s.spread = -1;
        s.color = Color::rgba(0, 0, 0, 25);
    }
    else if (name == L"lg") {
        s.offsetY = 10;
        s.blur = 15;
        s.spread = -3;
        s.color = Color::rgba(0, 0, 0, 25);
    }
    else if (name == L"xl") {
        s.offsetY = 20;
        s.blur = 25;
        s.spread = -5;
        s.color = Color::rgba(0, 0, 0, 25);
    }
    else if (name == L"2xl") {
        s.offsetY = 25;
        s.blur = 50;
        s.spread = -12;
        s.color = Color::rgba(0, 0, 0, 63);
    }
    else {
        return {};
    }

    return { s };
}

bool parseShadowArbitrary(const std::wstring& input, Shadow& out) {
    std::wstring text = input;

    for (wchar_t& c : text) {
        if (c == L'_')
            c = L' ';
    }

    size_t colorPos = text.find(L"rgb");

    std::wstring lengths = colorPos == std::wstring::npos
        ? text : text.substr(0, colorPos);

    std::wstring colorText = colorPos == std::wstring::npos
        ? L"rgba(0,0,0,0.25)" : text.substr(colorPos);

    float values[4] = { 0, 0, 0, 0 };
    int count = 0;
    size_t i = 0;

    while (i < lengths.size() && count < 4) {
        while (i < lengths.size() && iswspace(lengths[i]))
            ++i;

        if (i >= lengths.size())
            break;

        size_t start = i;

        while (i < lengths.size() && !iswspace(lengths[i]))
            ++i;

        float value = 0;
        bool percent = false;

        if (parseArbitraryLength(lengths.substr(start, i - start), 16.0f, value, percent))
            values[count++] = value;
    }

    if (count == 0)
        return false;

    out.offsetX = values[0];
    out.offsetY = values[1];
    out.blur = values[2];
    out.spread = values[3];

    if (!parseCssColor(colorText, out.color))
        out.color = Color::rgba(0, 0, 0, 64);

    return true;
}

} // namespace ui
