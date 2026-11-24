#include "style/class_table.h"

#include "core/utf.h"

#include <map>
#include <unordered_map>

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

Declaration color(Prop prop, Color value) {
    Declaration d;
    d.prop = prop;
    d.color = value;
    return d;
}

Declaration textValue(Prop prop, const std::wstring& value) {
    Declaration d;
    d.prop = prop;
    d.text = value;
    return d;
}

std::vector<Shadow> preset(const std::wstring& name) {
    Shadow s;
    s.color = Color::rgba(0, 0, 0, 26);

    if (name == L"sm") {
        s.offsetY = 1;
        s.blur = 2;
        s.color = Color::rgba(0, 0, 0, 13);
    }
    else if (name == L"md") {
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

Declaration shadows(const std::vector<Shadow>& value) {
    Declaration d;
    d.prop = Prop::Shadow;
    d.shadows = value;
    return d;
}

void textSize(Declarations& out, float px, float linePx) {
    out.push_back(num(Prop::FontSize, px));
    Declaration d = num(Prop::LineHeight, linePx);
    d.text.clear(); // px line height
    out.push_back(d);
}

const std::unordered_map<std::wstring, Declarations>& table() {
    static const std::unordered_map<std::wstring, Declarations> data = [] {
        std::unordered_map<std::wstring, Declarations> m;

        auto add = [&](const wchar_t* name, Declarations decls) {
            m[name] = std::move(decls);
        };

        add(L"flex", { num(Prop::Display, static_cast<float>(Display::Flex)) });
        add(L"hidden", { num(Prop::Display, static_cast<float>(Display::None)) });
        add(L"block", { num(Prop::Display, static_cast<float>(Display::Block)) });
        add(L"inline", { num(Prop::Display, static_cast<float>(Display::Inline)) });
        add(L"inline-block", { num(Prop::Display, static_cast<float>(Display::InlineBlock)) });

        add(L"relative", { num(Prop::Position, static_cast<float>(Position::Relative)) });
        add(L"absolute", { num(Prop::Position, static_cast<float>(Position::Absolute)) });

        add(L"flex-row", {
            num(Prop::Display, static_cast<float>(Display::Flex)),
            num(Prop::FlexDir, static_cast<float>(FlexDirection::Row))
        });
        add(L"flex-col", {
            num(Prop::Display, static_cast<float>(Display::Flex)),
            num(Prop::FlexDir, static_cast<float>(FlexDirection::Column))
        });
        add(L"flex-1", { num(Prop::Grow, 1) });
        add(L"flex-auto", { num(Prop::Grow, 1) });
        add(L"grow", { num(Prop::Grow, 1) });
        add(L"shrink-0", { num(Prop::Shrink, 0) });
        add(L"shrink", { num(Prop::Shrink, 1) });

        add(L"justify-start", { num(Prop::Justify, static_cast<float>(JustifyContent::Start)) });
        add(L"justify-center", { num(Prop::Justify, static_cast<float>(JustifyContent::Center)) });
        add(L"justify-end", { num(Prop::Justify, static_cast<float>(JustifyContent::End)) });
        add(L"justify-between", { num(Prop::Justify, static_cast<float>(JustifyContent::Between)) });
        add(L"justify-around", { num(Prop::Justify, static_cast<float>(JustifyContent::Around)) });
        add(L"justify-evenly", { num(Prop::Justify, static_cast<float>(JustifyContent::Evenly)) });

        add(L"items-start", { num(Prop::Align, static_cast<float>(AlignItems::Start)) });
        add(L"items-center", { num(Prop::Align, static_cast<float>(AlignItems::Center)) });
        add(L"items-end", { num(Prop::Align, static_cast<float>(AlignItems::End)) });
        add(L"items-stretch", { num(Prop::Align, static_cast<float>(AlignItems::Stretch)) });

        add(L"overflow-hidden", {
            num(Prop::OverflowX, static_cast<float>(Overflow::Hidden)),
            num(Prop::OverflowY, static_cast<float>(Overflow::Hidden))
        });
        add(L"overflow-y-auto", {
            num(Prop::OverflowX, static_cast<float>(Overflow::Hidden)),
            num(Prop::OverflowY, static_cast<float>(Overflow::Auto))
        });
        add(L"overflow-x-auto", {
            num(Prop::OverflowX, static_cast<float>(Overflow::Auto)),
            num(Prop::OverflowY, static_cast<float>(Overflow::Hidden))
        });

        add(L"text-xs", { num(Prop::FontSize, 12), num(Prop::LineHeight, 16) });
        add(L"text-sm", { num(Prop::FontSize, 14), num(Prop::LineHeight, 20) });
        add(L"text-base", { num(Prop::FontSize, 16), num(Prop::LineHeight, 24) });
        add(L"text-lg", { num(Prop::FontSize, 18), num(Prop::LineHeight, 28) });
        add(L"text-xl", { num(Prop::FontSize, 20), num(Prop::LineHeight, 28) });
        add(L"text-2xl", { num(Prop::FontSize, 24), num(Prop::LineHeight, 32) });
        add(L"text-3xl", { num(Prop::FontSize, 30), num(Prop::LineHeight, 36) });
        add(L"text-4xl", { num(Prop::FontSize, 36), num(Prop::LineHeight, 40) });
        add(L"text-5xl", { num(Prop::FontSize, 48), num(Prop::LineHeight, 48) });
        add(L"text-6xl", { num(Prop::FontSize, 60), num(Prop::LineHeight, 60) });
        add(L"text-7xl", { num(Prop::FontSize, 72), num(Prop::LineHeight, 72) });

        add(L"text-left", { num(Prop::TextAlign, static_cast<float>(TextAlign::Left)) });
        add(L"text-center", { num(Prop::TextAlign, static_cast<float>(TextAlign::Center)) });
        add(L"text-right", { num(Prop::TextAlign, static_cast<float>(TextAlign::Right)) });

        add(L"font-thin", { num(Prop::FontWeight, 100) });
        add(L"font-light", { num(Prop::FontWeight, 300) });
        add(L"font-normal", { num(Prop::FontWeight, 400) });
        add(L"font-medium", { num(Prop::FontWeight, 500) });
        add(L"font-semibold", { num(Prop::FontWeight, 600) });
        add(L"font-bold", { num(Prop::FontWeight, 700) });
        add(L"font-extrabold", { num(Prop::FontWeight, 800) });
        add(L"font-sans", { textValue(Prop::FontFamily, L"Segoe UI") });
        add(L"font-mono", { textValue(Prop::FontFamily, L"Consolas") });
        add(L"italic", { num(Prop::Italic, 1) });

        add(L"uppercase", { num(Prop::TextTransform, static_cast<float>(TextTransform::Uppercase)) });
        add(L"lowercase", { num(Prop::TextTransform, static_cast<float>(TextTransform::Lowercase)) });
        add(L"normal-case", { num(Prop::TextTransform, static_cast<float>(TextTransform::None)) });

        {
            Declaration d = num(Prop::LineHeight, 1.0f);
            d.text = L"m";
            add(L"leading-none", { d });
        }
        {
            Declaration d = num(Prop::LineHeight, 1.25f);
            d.text = L"m";
            add(L"leading-tight", { d });
        }
        {
            Declaration d = num(Prop::LineHeight, 1.375f);
            d.text = L"m";
            add(L"leading-snug", { d });
        }
        {
            Declaration d = num(Prop::LineHeight, 1.5f);
            d.text = L"m";
            add(L"leading-normal", { d });
        }
        {
            Declaration d = num(Prop::LineHeight, 1.625f);
            d.text = L"m";
            add(L"leading-relaxed", { d });
        }
        {
            Declaration d = num(Prop::LineHeight, 2.0f);
            d.text = L"m";
            add(L"leading-loose", { d });
        }

        add(L"tracking-tighter", { num(Prop::LetterSpacing, -0.8f) });
        add(L"tracking-tight", { num(Prop::LetterSpacing, -0.4f) });
        add(L"tracking-normal", { num(Prop::LetterSpacing, 0) });
        add(L"tracking-wide", { num(Prop::LetterSpacing, 0.4f) });
        add(L"tracking-wider", { num(Prop::LetterSpacing, 0.8f) });
        add(L"tracking-widest", { num(Prop::LetterSpacing, 1.6f) });

        add(L"rounded-none", {
            num(Prop::RadiusTL, 0), num(Prop::RadiusTR, 0),
            num(Prop::RadiusBR, 0), num(Prop::RadiusBL, 0)
        });
        add(L"rounded-sm", {
            num(Prop::RadiusTL, 2), num(Prop::RadiusTR, 2),
            num(Prop::RadiusBR, 2), num(Prop::RadiusBL, 2)
        });
        add(L"rounded", {
            num(Prop::RadiusTL, 4), num(Prop::RadiusTR, 4),
            num(Prop::RadiusBR, 4), num(Prop::RadiusBL, 4)
        });
        add(L"rounded-md", {
            num(Prop::RadiusTL, 6), num(Prop::RadiusTR, 6),
            num(Prop::RadiusBR, 6), num(Prop::RadiusBL, 6)
        });
        add(L"rounded-lg", {
            num(Prop::RadiusTL, 8), num(Prop::RadiusTR, 8),
            num(Prop::RadiusBR, 8), num(Prop::RadiusBL, 8)
        });
        add(L"rounded-xl", {
            num(Prop::RadiusTL, 12), num(Prop::RadiusTR, 12),
            num(Prop::RadiusBR, 12), num(Prop::RadiusBL, 12)
        });
        add(L"rounded-2xl", {
            num(Prop::RadiusTL, 16), num(Prop::RadiusTR, 16),
            num(Prop::RadiusBR, 16), num(Prop::RadiusBL, 16)
        });
        add(L"rounded-full", {
            num(Prop::RadiusTL, 9999), num(Prop::RadiusTR, 9999),
            num(Prop::RadiusBR, 9999), num(Prop::RadiusBL, 9999)
        });

        add(L"shadow-none", { shadows({}) });
        add(L"shadow-sm", { shadows(preset(L"sm")) });
        add(L"shadow", { shadows(preset(L"md")) });
        add(L"shadow-md", { shadows(preset(L"md")) });
        add(L"shadow-lg", { shadows(preset(L"lg")) });
        add(L"shadow-xl", { shadows(preset(L"xl")) });
        add(L"shadow-2xl", { shadows(preset(L"2xl")) });

        add(L"pointer-events-none", { num(Prop::PointerEvents, 0) });
        add(L"cursor-pointer", { num(Prop::Cursor, static_cast<float>(CursorKind::Hand)) });
        add(L"cursor-default", { num(Prop::Cursor, static_cast<float>(CursorKind::Arrow)) });
        add(L"cursor-text", { num(Prop::Cursor, static_cast<float>(CursorKind::IBeam)) });

        return m;
    }();

    return data;
}

} // namespace

bool lookupStaticClass(const std::wstring& cls, Declarations& out) {
    auto it = table().find(toLower(cls));

    if (it == table().end())
        return false;

    const Declarations& decls = it->second;
    out.insert(out.end(), decls.begin(), decls.end());
    return true;
}

bool isIgnoredClass(const std::wstring& input) {
    std::wstring cls = toLower(input);

    return cls == L"antialiased" ||
           cls == L"subpixel-antialiased" ||
           cls == L"select-none" ||
           cls == L"select-text" ||
           cls == L"outline-none" ||
           cls == L"appearance-none" ||
           cls == L"resize-none" ||
           cls == L"resize" ||
           // Semantic markers used by the info screen (template#example-row
           // binds .jp/.romaji/.en/.listen). They are hooks, not styles.
           cls == L"jp" ||
           cls == L"romaji" ||
           cls == L"en" ||
           cls == L"listen" ||
           cls.rfind(L"transition", 0) == 0 ||
           cls.rfind(L"duration-", 0) == 0 ||
           cls.rfind(L"ease-", 0) == 0 ||
           cls.rfind(L"delay-", 0) == 0 ||
           cls.rfind(L"ring-", 0) == 0 ||
           cls.rfind(L"focus:ring", 0) == 0 ||
           cls.rfind(L"bg-clip-", 0) == 0 ||
           cls.rfind(L"origin-", 0) == 0 ||
           cls.rfind(L"whitespace-", 0) == 0 ||
           cls.rfind(L"break-", 0) == 0;
}

} // namespace ui
