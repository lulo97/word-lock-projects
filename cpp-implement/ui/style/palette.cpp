#include "style/palette.h"

#include "core/utf.h"

#include <cwctype>
#include <cstdlib>
#include <map>
#include <sstream>

namespace ui {

namespace {

struct Rgb { uint8_t r, g, b; };

const std::map<std::wstring, Rgb>& palette() {
    static const std::map<std::wstring, Rgb> table = {
        { L"white",       { 255, 255, 255 } },
        { L"black",       { 0, 0, 0 } },

        { L"slate-50",    { 248, 250, 252 } },
        { L"slate-100",   { 241, 245, 249 } },
        { L"slate-200",   { 226, 232, 240 } },
        { L"slate-300",   { 203, 213, 225 } },
        { L"slate-400",   { 148, 163, 184 } },
        { L"slate-500",   { 100, 116, 139 } },
        { L"slate-600",   { 71, 85, 105 } },
        { L"slate-700",   { 51, 65, 85 } },
        { L"slate-800",   { 30, 41, 59 } },
        { L"slate-900",   { 15, 23, 42 } },
        { L"slate-950",   { 2, 6, 23 } },

        { L"gray-50",     { 249, 250, 251 } },
        { L"gray-100",    { 243, 244, 246 } },
        { L"gray-200",    { 229, 231, 235 } },
        { L"gray-300",    { 209, 213, 219 } },
        { L"gray-400",    { 156, 163, 175 } },
        { L"gray-500",    { 107, 114, 128 } },
        { L"gray-600",    { 75, 85, 99 } },
        { L"gray-700",    { 55, 65, 81 } },
        { L"gray-800",    { 31, 41, 55 } },
        { L"gray-900",    { 17, 24, 39 } },
        { L"gray-950",    { 3, 7, 18 } },

        { L"blue-50",     { 239, 246, 255 } },
        { L"blue-100",    { 219, 234, 254 } },
        { L"blue-200",    { 191, 219, 254 } },
        { L"blue-300",    { 147, 197, 253 } },
        { L"blue-400",    { 96, 165, 250 } },
        { L"blue-500",    { 59, 130, 246 } },
        { L"blue-600",    { 37, 99, 235 } },
        { L"blue-700",    { 29, 78, 216 } },
        { L"blue-800",    { 30, 64, 175 } },
        { L"blue-900",    { 30, 58, 138 } },

        { L"sky-50",      { 240, 249, 255 } },
        { L"sky-100",     { 224, 242, 254 } },
        { L"sky-200",     { 186, 230, 253 } },
        { L"sky-300",     { 125, 211, 252 } },
        { L"sky-400",     { 56, 189, 248 } },
        { L"sky-500",     { 14, 165, 233 } },
        { L"sky-600",     { 2, 132, 199 } },
        { L"sky-700",     { 3, 105, 161 } },
        { L"sky-800",     { 7, 89, 133 } },
        { L"sky-900",     { 12, 74, 110 } },

        { L"red-50",      { 254, 242, 242 } },
        { L"red-100",     { 254, 226, 226 } },
        { L"red-200",     { 254, 202, 202 } },
        { L"red-300",     { 252, 165, 165 } },
        { L"red-400",     { 248, 113, 113 } },
        { L"red-500",     { 239, 68, 68 } },
        { L"red-600",     { 220, 38, 38 } },
        { L"red-700",     { 185, 28, 28 } },
        { L"red-800",     { 153, 27, 27 } },
        { L"red-900",     { 127, 29, 29 } },

        { L"green-50",    { 240, 253, 244 } },
        { L"green-100",   { 220, 252, 231 } },
        { L"green-200",   { 187, 247, 208 } },
        { L"green-300",   { 134, 239, 172 } },
        { L"green-400",   { 74, 222, 128 } },
        { L"green-500",   { 34, 197, 94 } },
        { L"green-600",   { 22, 163, 74 } },
        { L"green-700",   { 21, 128, 61 } },
        { L"green-800",   { 22, 101, 52 } },
        { L"green-900",   { 20, 83, 45 } },
    };

    return table;
}

int parseInt(const std::wstring& s) {
    return _wtoi(s.c_str());
}

} // namespace

bool lookupPaletteColor(const std::wstring& name, Color& out) {
    if (name == L"transparent") {
        out = Color::transparent();
        return true;
    }

    auto it = palette().find(toLower(name));

    if (it == palette().end())
        return false;

    out = Color::rgb(it->second.r, it->second.g, it->second.b);
    return true;
}

bool parseCssColor(const std::wstring& input, Color& out) {
    std::wstring text = trim(input);

    if (text.size() >= 2 && text.front() == L'[' && text.back() == L']')
        text = text.substr(1, text.size() - 2);

    if (text.empty())
        return false;

    if (text[0] == L'#') {
        std::wstring hex = text.substr(1);

        auto nibble = [](wchar_t c, int& value) {
            if (c >= L'0' && c <= L'9') { value = c - L'0'; return true; }
            if (c >= L'a' && c <= L'f') { value = c - L'a' + 10; return true; }
            if (c >= L'A' && c <= L'F') { value = c - L'A' + 10; return true; }
            return false;
        };

        auto byteAt = [&](size_t pos, uint8_t& value) {
            int hi = 0, lo = 0;

            if (pos + 1 >= hex.size() ||
                !nibble(hex[pos], hi) ||
                !nibble(hex[pos + 1], lo))
                return false;

            value = static_cast<uint8_t>(hi * 16 + lo);
            return true;
        };

        if (hex.size() == 3) {
            int r, g, b;

            if (!nibble(hex[0], r) || !nibble(hex[1], g) || !nibble(hex[2], b))
                return false;

            out = Color::rgb(
                static_cast<uint8_t>(r * 17),
                static_cast<uint8_t>(g * 17),
                static_cast<uint8_t>(b * 17)
            );
            return true;
        }

        if (hex.size() == 4) {
            int r, g, b, a;

            if (!nibble(hex[0], r) || !nibble(hex[1], g) ||
                !nibble(hex[2], b) || !nibble(hex[3], a))
                return false;

            out = Color::rgba(
                static_cast<uint8_t>(r * 17),
                static_cast<uint8_t>(g * 17),
                static_cast<uint8_t>(b * 17),
                static_cast<uint8_t>(a * 17)
            );
            return true;
        }

        if (hex.size() == 6 || hex.size() == 8) {
            uint8_t r, g, b, a = 255;

            if (!byteAt(0, r) || !byteAt(2, g) || !byteAt(4, b))
                return false;

            if (hex.size() == 8 && !byteAt(6, a))
                return false;

            out = Color::rgba(r, g, b, a);
            return true;
        }

        return false;
    }

    std::wstring lower = toLower(text);

    if (lower.rfind(L"rgb", 0) == 0) {
        size_t open = lower.find(L'(');
        size_t close = lower.rfind(L')');

        if (open == std::wstring::npos || close == std::wstring::npos || close <= open)
            return false;

        std::wstring inside = lower.substr(open + 1, close - open - 1);

        for (wchar_t& c : inside)
            if (c == L',')
                c = L' ';

        std::wistringstream in(inside);
        float r = 0, g = 0, b = 0, a = 1;

        if (!(in >> r >> g >> b))
            return false;

        if (!(in >> a))
            a = 1;

        if (a <= 1.0f)
            a *= 255.0f;

        out = Color::rgba(
            static_cast<uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r)),
            static_cast<uint8_t>(g < 0 ? 0 : (g > 255 ? 255 : g)),
            static_cast<uint8_t>(b < 0 ? 0 : (b > 255 ? 255 : b)),
            static_cast<uint8_t>(a < 0 ? 0 : (a > 255 ? 255 : a))
        );
        return true;
    }

    return lookupPaletteColor(lower, out);
}

bool parseColorToken(const std::wstring& input, Color& out) {
    std::wstring token = trim(input);

    if (token.size() >= 2 && token.front() == L'[' && token.back() == L']')
        return parseCssColor(token, out);

    std::wstring base = token;
    float alpha = 255.0f;

    size_t slash = token.find_last_of(L'/');

    if (slash != std::wstring::npos) {
        std::wstring alphaText = token.substr(slash + 1);
        float value = 0;

        if (parseFloat(alphaText, value)) {
            if (value > 0.0f && value < 1.0f)
                alpha = value * 255.0f;
            else
                alpha = value * 2.55f; // Tailwind opacity suffix: /30 => 30%

            base = token.substr(0, slash);
        }
    }

    Color color;

    if (!parseCssColor(base, color))
        return false;

    color.a = static_cast<uint8_t>(
        alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha)
    );
    out = color;
    return true;
}

} // namespace ui
