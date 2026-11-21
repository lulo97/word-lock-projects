#include "style/spacing.h"

#include "core/utf.h"

#include <map>

namespace ui {

namespace {

const std::map<std::wstring, float>& spacingScale() {
    static const std::map<std::wstring, float> table = {
        { L"0", 0.0f },       { L"px", 1.0f },     { L"0.5", 2.0f },
        { L"1", 4.0f },       { L"1.5", 6.0f },    { L"2", 8.0f },
        { L"2.5", 10.0f },    { L"3", 12.0f },     { L"3.5", 14.0f },
        { L"4", 16.0f },      { L"5", 20.0f },     { L"6", 24.0f },
        { L"7", 28.0f },      { L"8", 32.0f },     { L"9", 36.0f },
        { L"10", 40.0f },     { L"11", 44.0f },    { L"12", 48.0f },
        { L"14", 56.0f },     { L"16", 64.0f },    { L"20", 80.0f },
        { L"24", 96.0f },     { L"28", 112.0f },   { L"32", 128.0f },
        { L"36", 144.0f },    { L"40", 160.0f },   { L"44", 176.0f },
        { L"48", 192.0f },    { L"52", 208.0f },   { L"56", 224.0f },
        { L"60", 240.0f },    { L"64", 256.0f },   { L"72", 288.0f },
        { L"80", 320.0f },    { L"96", 384.0f }
    };

    return table;
}

} // namespace

bool parseSpacingScale(const std::wstring& key, float& px) {
    auto it = spacingScale().find(trim(key));

    if (it == spacingScale().end())
        return false;

    px = it->second;
    return true;
}

bool parseArbitraryLength(
    const std::wstring& input,
    float emBasis,
    float& value,
    bool& isPercent
) {
    std::wstring text = trim(input);

    if (text.size() >= 2 && text.front() == L'[' && text.back() == L']')
        text = text.substr(1, text.size() - 2);

    for (wchar_t& c : text)
        if (c == L'_')
            c = L' ';

    text = trim(text);
    isPercent = false;

    if (text.empty())
        return false;

    if (text.back() == L'%') {
        float number = 0;

        if (!parseFloat(text.substr(0, text.size() - 1), number))
            return false;

        value = number;
        isPercent = true;
        return true;
    }

    float multiplier = 1.0f;
    std::wstring numberText = text;

    if (text.size() > 2 && text.substr(text.size() - 2) == L"px") {
        numberText = text.substr(0, text.size() - 2);
    }
    else if (text.size() > 3 && text.substr(text.size() - 3) == L"rem") {
        numberText = text.substr(0, text.size() - 3);
        multiplier = 16.0f;
    }
    else if (text.size() > 2 && text.substr(text.size() - 2) == L"em") {
        numberText = text.substr(0, text.size() - 2);
        multiplier = emBasis > 0 ? emBasis : 16.0f;
    }

    float number = 0;

    if (!parseFloat(numberText, number))
        return false;

    value = number * multiplier;
    return true;
}

bool parseFraction(const std::wstring& key, float& percent) {
    size_t slash = key.find(L'/');

    if (slash == std::wstring::npos)
        return false;

    float numerator = 0, denominator = 0;

    if (!parseFloat(key.substr(0, slash), numerator) ||
        !parseFloat(key.substr(slash + 1), denominator) ||
        denominator == 0)
        return false;

    percent = numerator / denominator * 100.0f;
    return true;
}

bool parseSizeToken(const std::wstring& input, Length& out) {
    std::wstring key = trim(input);

    if (key.empty())
        return false;

    if (key == L"full") {
        out = Length::percent(100);
        return true;
    }

    if (key == L"screen") {
        out.unit = Length::Unit::Screen;
        out.value = 100;
        return true;
    }

    if (key == L"auto") {
        out = Length{};
        return true;
    }

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

    float px = 0;

    if (parseSpacingScale(key, px)) {
        out = Length::px(px);
        return true;
    }

    return false;
}

} // namespace ui
