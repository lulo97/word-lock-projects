#include "style/class_rules.h"

#include "core/utf.h"
#include "style/palette.h"
#include "style/spacing.h"

#include <cstdlib>

namespace ui {

namespace {

Declaration num(Prop prop, float value) {
    Declaration d;
    d.prop = prop;
    d.number = value;
    return d;
}

Declaration len(Prop prop, const Length& value) {
    Declaration d;
    d.prop = prop;
    d.length = value;
    return d;
}

Declaration colorDecl(Prop prop, Color value) {
    Declaration d;
    d.prop = prop;
    d.color = value;
    return d;
}

Declaration textDecl(Prop prop, const std::wstring& value) {
    Declaration d;
    d.prop = prop;
    d.text = value;
    return d;
}

bool starts(const std::wstring& s, const wchar_t* prefix) {
    return s.rfind(prefix, 0) == 0;
}

bool arbitrary(const std::wstring& key, std::wstring& inner) {
    if (key.size() >= 2 && key.front() == L'[' && key.back() == L']') {
        inner = key.substr(1, key.size() - 2);
        return true;
    }

    return false;
}

bool lengthToken(const std::wstring& key, Length& out) {
    if (key.empty())
        return false;

    if (key.size() >= 2 && key.front() == L'[' && key.back() == L']') {
        float value = 0;
        bool percent = false;

        if (!parseArbitraryLength(key, 16.0f, value, percent))
            return false;

        out = percent ? Length::percent(value) : Length::px(value);
        return true;
    }

    float fraction = 0;

    if (parseFraction(key, fraction)) {
        out = Length::percent(fraction);
        return true;
    }

    if (key == L"full") {
        out = Length::percent(100);
        return true;
    }

    if (key == L"screen") {
        out.unit = Length::Unit::Screen;
        out.value = 100;
        return true;
    }

    float px = 0;

    if (parseSpacingScale(key, px)) {
        out = Length::px(px);
        return true;
    }

    return false;
}

bool spacingToken(const std::wstring& key, float& px) {
    if (key.empty())
        return false;

    if (key.size() >= 2 && key.front() == L'[' && key.back() == L']') {
        float value = 0;
        bool percent = false;

        if (!parseArbitraryLength(key, 16.0f, value, percent) || percent)
            return false;

        px = value;
        return true;
    }

    return parseSpacingScale(key, px);
}

bool radiusToken(const std::wstring& key, float& px) {
    if (key.empty()) { px = 4; return true; }
    if (key == L"none") { px = 0; return true; }
    if (key == L"sm") { px = 2; return true; }
    if (key == L"md") { px = 6; return true; }
    if (key == L"lg") { px = 8; return true; }
    if (key == L"xl") { px = 12; return true; }
    if (key == L"2xl") { px = 16; return true; }
    if (key == L"3xl") { px = 24; return true; }
    if (key == L"full") { px = 9999; return true; }

    if (key.size() >= 2 && key.front() == L'[' && key.back() == L']') {
        float value = 0;
        bool percent = false;

        if (!parseArbitraryLength(key, 16.0f, value, percent))
            return false;

        px = value;
        return true;
    }

    return false;
}

float numberToken(const std::wstring& key, float fallback = -1) {
    std::wstring text = key;

    if (text.size() >= 2 && text.front() == L'[' && text.back() == L']')
        text = text.substr(1, text.size() - 2);

    if (!text.empty() && text.back() == L'p') {
        size_t end = text.find_last_not_of(L"0123456789.");
        if (end != std::wstring::npos && end + 1 < text.size())
            text = text.substr(0, end + 1);
    }

    if (!text.empty() && text.back() == L'x')
        text.pop_back();

    float value = 0;

    if (!parseFloat(text, value))
        return fallback;

    return value;
}

bool parseShadowInner(const std::wstring& input, Shadow& out) {
    std::wstring text = input;

    for (wchar_t& c : text)
        if (c == L'_')
            c = L' ';

    size_t rgbPos = text.find(L"rgb");

    std::wstring lengths = rgbPos == std::wstring::npos
        ? text : text.substr(0, rgbPos);

    std::wstring colorText = rgbPos == std::wstring::npos
        ? L"rgba(0,0,0,0.25)" : text.substr(rgbPos);

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

    out.offsetX = values[0];
    out.offsetY = values[1];
    out.blur = values[2];
    out.spread = values[3];

    Color c;

    if (parseCssColor(colorText, c))
        out.color = c;
    else
        out.color = Color::rgba(0, 0, 0, 64);

    return true;
}

} // namespace

bool ruleSpacing(const std::wstring& input, Declarations& out) {
    std::wstring cls = input;
    float sign = 1;

    if (!cls.empty() && cls[0] == L'-') {
        sign = -1;
        cls.erase(cls.begin());
    }

    struct Prefix {
        const wchar_t* name;
        Prop top, right, bottom, left;
        bool all;
    };

    static const Prefix prefixes[] = {
        { L"p-",  Prop::PaddingT, Prop::PaddingR, Prop::PaddingB, Prop::PaddingL, true },
        { L"px-", Prop::PaddingL, Prop::PaddingR, Prop::PaddingL, Prop::PaddingR, false },
        { L"py-", Prop::PaddingT, Prop::PaddingT, Prop::PaddingB, Prop::PaddingB, false },
        { L"pt-", Prop::PaddingT, Prop::PaddingT, Prop::PaddingT, Prop::PaddingT, false },
        { L"pr-", Prop::PaddingR, Prop::PaddingR, Prop::PaddingR, Prop::PaddingR, false },
        { L"pb-", Prop::PaddingB, Prop::PaddingB, Prop::PaddingB, Prop::PaddingB, false },
        { L"pl-", Prop::PaddingL, Prop::PaddingL, Prop::PaddingL, Prop::PaddingL, false },
        { L"m-",  Prop::MarginT, Prop::MarginR, Prop::MarginB, Prop::MarginL, true },
        { L"mx-", Prop::MarginL, Prop::MarginR, Prop::MarginL, Prop::MarginR, false },
        { L"my-", Prop::MarginT, Prop::MarginT, Prop::MarginB, Prop::MarginB, false },
        { L"mt-", Prop::MarginT, Prop::MarginT, Prop::MarginT, Prop::MarginT, false },
        { L"mr-", Prop::MarginR, Prop::MarginR, Prop::MarginR, Prop::MarginR, false },
        { L"mb-", Prop::MarginB, Prop::MarginB, Prop::MarginB, Prop::MarginB, false },
        { L"ml-", Prop::MarginL, Prop::MarginL, Prop::MarginL, Prop::MarginL, false }
    };

    for (const Prefix& p : prefixes) {
        if (!starts(cls, p.name))
            continue;

        float value = 0;

        if (!spacingToken(cls.substr(wcslen(p.name)), value))
            return false;

        value *= sign;

        if (p.all) {
            out.push_back(num(p.top, value));
            out.push_back(num(p.right, value));
            out.push_back(num(p.bottom, value));
            out.push_back(num(p.left, value));
        }
        else {
            out.push_back(num(p.top, value));
            out.push_back(num(p.right, value));
            out.push_back(num(p.bottom, value));
            out.push_back(num(p.left, value));
        }

        return true;
    }

    if (starts(cls, L"gap-x-")) {
        float value = 0;

        if (!spacingToken(cls.substr(6), value))
            return false;

        out.push_back(num(Prop::GapX, value));
        return true;
    }

    if (starts(cls, L"gap-y-")) {
        float value = 0;

        if (!spacingToken(cls.substr(6), value))
            return false;

        out.push_back(num(Prop::GapY, value));
        return true;
    }

    if (starts(cls, L"gap-")) {
        float value = 0;

        if (!spacingToken(cls.substr(4), value))
            return false;

        out.push_back(num(Prop::GapX, value));
        out.push_back(num(Prop::GapY, value));
        return true;
    }

    if (starts(cls, L"space-x-")) {
        float value = 0;

        if (!spacingToken(cls.substr(8), value))
            return false;

        out.push_back(num(Prop::SpaceX, value));
        return true;
    }

    if (starts(cls, L"space-y-")) {
        float value = 0;

        if (!spacingToken(cls.substr(8), value))
            return false;

        out.push_back(num(Prop::SpaceY, value));
        return true;
    }

    return false;
}

bool ruleSizing(const std::wstring& cls, Declarations& out) {
    struct Prefix { const wchar_t* name; Prop prop; bool screenMin; };
    static const Prefix prefixes[] = {
        { L"min-w-", Prop::MinWidth, false },
        { L"min-h-", Prop::MinHeight, true },
        { L"max-w-", Prop::MaxWidth, false },
        { L"max-h-", Prop::MaxHeight, true },
        { L"w-",     Prop::Width, false },
        { L"h-",     Prop::Height, true }
    };

    for (const Prefix& p : prefixes) {
        if (!starts(cls, p.name))
            continue;

        std::wstring key = cls.substr(wcslen(p.name));

        if (p.name == std::wstring(L"min-h-") && key == L"screen") {
            Length screenLength;
            screenLength.unit = Length::Unit::Screen;
            screenLength.value = 100;
            out.push_back(len(p.prop, screenLength));
            return true;
        }

        if (p.name == std::wstring(L"max-w-")) {
            static const std::pair<const wchar_t*, float> widths[] = {
                { L"xs", 320.0f },  { L"sm", 384.0f },  { L"md", 448.0f },
                { L"lg", 512.0f },  { L"xl", 576.0f },  { L"2xl", 672.0f },
                { L"3xl", 768.0f }, { L"4xl", 896.0f }, { L"5xl", 1024.0f },
                { L"6xl", 1152.0f }, { L"7xl", 1280.0f }
            };

            for (const auto& w : widths) {
                if (key == w.first) {
                    out.push_back(len(p.prop, Length::px(w.second)));
                    return true;
                }
            }
        }

        std::wstring inner;
        if (arbitrary(key, inner)) {
            std::wstring t = trim(inner);
            for (wchar_t& c : t) if (c == L'_') c = L' ';
            t = trim(t);
            if (t.size() > 2 && t.substr(t.size() - 2) == L"vh") {
                float n = 0;
                if (parseFloat(t.substr(0, t.size() - 2), n)) {
                    Length screenLength;
                    screenLength.unit = Length::Unit::Screen;
                    screenLength.value = n;
                    out.push_back(len(p.prop, screenLength));
                    return true;
                }
            }
        }

        Length value;

        if (!lengthToken(key, value))
            return false;

        out.push_back(len(p.prop, value));
        return true;
    }

    return false;
}

bool ruleText(const std::wstring& cls, Declarations& out) {
    if (!starts(cls, L"text-"))
        return false;

    std::wstring key = cls.substr(5);

    if (key.empty())
        return false;

    std::wstring inner;

    if (arbitrary(key, inner)) {
        Color c;

        if ((!inner.empty() && (inner[0] == L'#' || starts(toLower(inner), L"rgb"))) &&
            parseCssColor(inner, c)) {
            out.push_back(colorDecl(Prop::Color, c));
            return true;
        }

        float value = 0;
        bool percent = false;

        if (parseArbitraryLength(inner, 16.0f, value, percent) && !percent) {
            out.push_back(num(Prop::FontSize, value));
            return true;
        }

        return false;
    }

    Color c;

    if (parseColorToken(key, c)) {
        out.push_back(colorDecl(Prop::Color, c));
        return true;
    }

    return false;
}

bool ruleFont(const std::wstring& cls, Declarations& out) {
    if (starts(cls, L"font-[")) {
        std::wstring inner;

        if (!arbitrary(cls.substr(5), inner))
            return false;

        out.push_back(textDecl(Prop::FontFamily, inner));
        return true;
    }

    return false;
}

bool ruleLeadingTracking(const std::wstring& cls, Declarations& out) {
    if (starts(cls, L"leading-")) {
        std::wstring key = cls.substr(8);
        std::wstring inner;

        if (arbitrary(key, inner)) {
            float value = 0;
            bool percent = false;

            if (!parseArbitraryLength(inner, 16.0f, value, percent))
                return false;

            if (percent) {
                out.push_back(num(Prop::LineHeight, value / 100.0f));
                out.push_back(textDecl(Prop::LineHeight, L"m"));
            }
            else {
                out.push_back(num(Prop::LineHeight, value));
            }

            return true;
        }

        return false;
    }

    if (starts(cls, L"tracking-")) {
        std::wstring key = cls.substr(9);
        std::wstring inner;

        if (!arbitrary(key, inner))
            return false;

        float value = 0;
        bool percent = false;

        if (!parseArbitraryLength(inner, 16.0f, value, percent))
            return false;

        out.push_back(num(Prop::LetterSpacing, value));
        return true;
    }

    return false;
}

bool ruleBackground(const std::wstring& cls, Declarations& out) {
    if (!starts(cls, L"bg-"))
        return false;

    std::wstring key = cls.substr(3);

    if (key.empty() || starts(key, L"gradient-") || starts(key, L"clip-"))
        return false;

    Color c;

    if (!parseColorToken(key, c))
        return false;

    out.push_back(colorDecl(Prop::Background, c));
    return true;
}

bool ruleBorder(const std::wstring& input, Declarations& out) {
    if (!starts(input, L"border"))
        return false;

    std::wstring cls = input;

    if (cls == L"border") {
        out.push_back(num(Prop::BorderWidthT, 1));
        out.push_back(num(Prop::BorderWidthR, 1));
        out.push_back(num(Prop::BorderWidthB, 1));
        out.push_back(num(Prop::BorderWidthL, 1));
        return true;
    }

    if (cls == L"border-t") { out.push_back(num(Prop::BorderWidthT, 1)); return true; }
    if (cls == L"border-r") { out.push_back(num(Prop::BorderWidthR, 1)); return true; }
    if (cls == L"border-b") { out.push_back(num(Prop::BorderWidthB, 1)); return true; }
    if (cls == L"border-l") { out.push_back(num(Prop::BorderWidthL, 1)); return true; }

    std::wstring key;

    if (starts(cls, L"border-t-")) { key = cls.substr(9); }
    else if (starts(cls, L"border-r-")) { key = cls.substr(9); }
    else if (starts(cls, L"border-b-")) { key = cls.substr(9); }
    else if (starts(cls, L"border-l-")) { key = cls.substr(9); }
    else if (starts(cls, L"border-")) { key = cls.substr(7); }
    else return false;

    if (key.empty())
        return false;

    bool width = false;
    float widthPx = 0;

    if (key.size() >= 2 && key.front() == L'[' && key.back() == L']') {
        std::wstring inner = key.substr(1, key.size() - 2);
        Color c;

        if (parseCssColor(inner, c)) {
            out.push_back(colorDecl(Prop::BorderColor, c));
            return true;
        }

        bool percent = false;
        if (parseArbitraryLength(inner, 16.0f, widthPx, percent) && !percent) {
            width = true;
        }
    }
    else {
        float value = numberToken(key);

        if (value >= 0 && key.find_first_not_of(L"0123456789.") == std::wstring::npos) {
            width = true;
            widthPx = value;
        }
    }

    if (width) {
        std::wstring prefix = cls.substr(0, cls.size() - key.size());

        if (prefix == L"border-t-") out.push_back(num(Prop::BorderWidthT, widthPx));
        else if (prefix == L"border-r-") out.push_back(num(Prop::BorderWidthR, widthPx));
        else if (prefix == L"border-b-") out.push_back(num(Prop::BorderWidthB, widthPx));
        else if (prefix == L"border-l-") out.push_back(num(Prop::BorderWidthL, widthPx));
        else {
            out.push_back(num(Prop::BorderWidthT, widthPx));
            out.push_back(num(Prop::BorderWidthR, widthPx));
            out.push_back(num(Prop::BorderWidthB, widthPx));
            out.push_back(num(Prop::BorderWidthL, widthPx));
        }

        return true;
    }

    Color c;

    if (!parseColorToken(key, c))
        return false;

    out.push_back(colorDecl(Prop::BorderColor, c));
    return true;
}

bool ruleRounded(const std::wstring& cls, Declarations& out) {
    if (!starts(cls, L"rounded"))
        return false;

    std::wstring key = cls.substr(7);

    if (!key.empty() && key[0] == L'-')
        return false;

    if (key.empty()) {
        out.push_back(num(Prop::RadiusTL, 4));
        out.push_back(num(Prop::RadiusTR, 4));
        out.push_back(num(Prop::RadiusBR, 4));
        out.push_back(num(Prop::RadiusBL, 4));
        return true;
    }

    if (starts(key, L"t-")) {
        float radius = 0;
        if (!radiusToken(key.substr(2), radius)) return false;
        out.push_back(num(Prop::RadiusTL, radius));
        out.push_back(num(Prop::RadiusTR, radius));
        return true;
    }

    if (starts(key, L"r-")) {
        float radius = 0;
        if (!radiusToken(key.substr(2), radius)) return false;
        out.push_back(num(Prop::RadiusTR, radius));
        out.push_back(num(Prop::RadiusBR, radius));
        return true;
    }

    if (starts(key, L"b-")) {
        float radius = 0;
        if (!radiusToken(key.substr(2), radius)) return false;
        out.push_back(num(Prop::RadiusBR, radius));
        out.push_back(num(Prop::RadiusBL, radius));
        return true;
    }

    if (starts(key, L"l-")) {
        float radius = 0;
        if (!radiusToken(key.substr(2), radius)) return false;
        out.push_back(num(Prop::RadiusTL, radius));
        out.push_back(num(Prop::RadiusBL, radius));
        return true;
    }

    if (starts(key, L"tl-")) {
        float radius = 0;
        if (!radiusToken(key.substr(3), radius)) return false;
        out.push_back(num(Prop::RadiusTL, radius));
        return true;
    }

    if (starts(key, L"tr-")) {
        float radius = 0;
        if (!radiusToken(key.substr(3), radius)) return false;
        out.push_back(num(Prop::RadiusTR, radius));
        return true;
    }

    if (starts(key, L"br-")) {
        float radius = 0;
        if (!radiusToken(key.substr(3), radius)) return false;
        out.push_back(num(Prop::RadiusBR, radius));
        return true;
    }

    if (starts(key, L"bl-")) {
        float radius = 0;
        if (!radiusToken(key.substr(3), radius)) return false;
        out.push_back(num(Prop::RadiusBL, radius));
        return true;
    }

    float radius = 0;

    if (!radiusToken(key, radius))
        return false;

    out.push_back(num(Prop::RadiusTL, radius));
    out.push_back(num(Prop::RadiusTR, radius));
    out.push_back(num(Prop::RadiusBR, radius));
    out.push_back(num(Prop::RadiusBL, radius));
    return true;
}

bool ruleShadow(const std::wstring& cls, Declarations& out) {
    if (!starts(cls, L"shadow-[") || cls.back() != L']')
        return false;

    std::wstring inner = cls.substr(8, cls.size() - 9);
    Shadow shadow;

    if (!parseShadowInner(inner, shadow))
        return false;

    Declaration d;
    d.prop = Prop::Shadow;
    d.shadows.push_back(shadow);
    out.push_back(std::move(d));
    return true;
}

bool rulePosition(const std::wstring& input, Declarations& out) {
    std::wstring cls = input;
    float sign = 1;

    if (starts(cls, L"-translate-")) {
        sign = -1;
        cls.erase(cls.begin());
    }

    if (starts(cls, L"translate-x-")) {
        std::wstring key = cls.substr(12);
        float fraction = 0;

        if (!parseFraction(key, fraction))
            return false;

        out.push_back(num(Prop::TranslateX, sign * fraction));
        return true;
    }

    if (starts(cls, L"translate-y-")) {
        std::wstring key = cls.substr(12);
        float fraction = 0;

        if (!parseFraction(key, fraction))
            return false;

        out.push_back(num(Prop::TranslateY, sign * fraction));
        return true;
    }

    if (starts(cls, L"z-")) {
        float value = 0;

        if (!parseFloat(cls.substr(2), value))
            return false;

        out.push_back(num(Prop::ZIndex, value));
        return true;
    }

    struct Prefix { const wchar_t* name; Prop prop; };
    static const Prefix prefixes[] = {
        { L"inset-x-", Prop::Left },
        { L"inset-y-", Prop::Top },
        { L"left-", Prop::Left },
        { L"right-", Prop::Right },
        { L"top-", Prop::Top },
        { L"bottom-", Prop::Bottom },
        { L"inset-", Prop::Top }
    };

    for (const Prefix& p : prefixes) {
        if (!starts(cls, p.name))
            continue;

        std::wstring key = cls.substr(wcslen(p.name));
        Length value;

        if (!lengthToken(key, value))
            return false;

        if (p.name == std::wstring(L"inset-")) {
            out.push_back(len(Prop::Top, value));
            out.push_back(len(Prop::Right, value));
            out.push_back(len(Prop::Bottom, value));
            out.push_back(len(Prop::Left, value));
        }
        else if (p.name == std::wstring(L"inset-x-")) {
            out.push_back(len(Prop::Left, value));
            out.push_back(len(Prop::Right, value));
        }
        else if (p.name == std::wstring(L"inset-y-")) {
            out.push_back(len(Prop::Top, value));
            out.push_back(len(Prop::Bottom, value));
        }
        else {
            out.push_back(len(p.prop, value));
        }

        return true;
    }

    return false;
}

bool ruleFlex(const std::wstring& cls, Declarations& out) {
    if (cls == L"flex-1") {
        out.push_back(num(Prop::Grow, 1));
        return true;
    }

    if (starts(cls, L"flex-[")) {
        std::wstring inner;
        if (!arbitrary(cls.substr(5), inner))
            return false;

        float value = 0;
        if (!parseFloat(inner, value))
            return false;

        out.push_back(num(Prop::Grow, value));
        return true;
    }

    return false;
}

bool applyGenericClass(const std::wstring& cls, Declarations& out) {
    if (cls.empty())
        return false;

    if (ruleSpacing(cls, out)) return true;
    if (ruleSizing(cls, out)) return true;
    if (ruleText(cls, out)) return true;
    if (ruleFont(cls, out)) return true;
    if (ruleLeadingTracking(cls, out)) return true;
    if (ruleBackground(cls, out)) return true;
    if (ruleBorder(cls, out)) return true;
    if (ruleRounded(cls, out)) return true;
    if (ruleShadow(cls, out)) return true;
    if (rulePosition(cls, out)) return true;
    if (ruleFlex(cls, out)) return true;

    return false;
}

} // namespace ui
