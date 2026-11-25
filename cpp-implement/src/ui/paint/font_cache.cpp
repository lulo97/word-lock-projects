#include "paint/font_cache.h"

#include "core/utf.h"

#include <map>
#include <mutex>
#include <utility>

namespace ui {

namespace {

std::recursive_mutex g_mutex;
std::map<FontKey, HFONT> g_fonts;
std::map<FontKey, FontMetrics> g_metrics;

HDC measureDc() {
    static HDC dc = [] {
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        ReleaseDC(nullptr, screen);
        return mem;
    }();
    return dc;
}

bool keyLess(const FontKey& a, const FontKey& b) {
    if (a.family != b.family) return a.family < b.family;
    if (a.px != b.px) return a.px < b.px;
    if (a.weight != b.weight) return a.weight < b.weight;
    return a.italic < b.italic;
}

std::wstring chooseFamily(const std::wstring& family, wchar_t c) {
    if (isCjk(c)) {
        if (family == L"Yu Gothic UI" || family == L"Meiryo" ||
            family == L"Microsoft YaHei UI")
            return family;

        return L"Yu Gothic UI";
    }

    return family.empty() ? L"Segoe UI" : family;
}

} // namespace

bool FontKey::operator==(const FontKey& o) const {
    return px == o.px && weight == o.weight &&
           italic == o.italic && family == o.family;
}

bool FontKey::operator<(const FontKey& o) const {
    return keyLess(*this, o);
}

HFONT getFont(const FontKey& key) {
    std::lock_guard<std::recursive_mutex> lock(g_mutex);

    auto it = g_fonts.find(key);

    if (it != g_fonts.end())
        return it->second;

    std::wstring family = key.family.empty() ? L"Segoe UI" : key.family;

    HFONT font = CreateFontW(
        -key.px,
        0, 0, 0,
        key.weight,
        key.italic ? TRUE : FALSE,
        FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        family.c_str()
    );

    if (!font)
        font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    g_fonts[key] = font;
    return font;
}

std::vector<std::wstring> fallbackChain(const std::wstring& family) {
    std::vector<std::wstring> chain;

    if (!family.empty())
        chain.push_back(family);

    const wchar_t* defaults[] = {
        L"Segoe UI", L"Yu Gothic UI", L"Meiryo",
        L"Microsoft YaHei UI", L"Arial"
    };

    for (const wchar_t* name : defaults) {
        bool exists = false;

        for (const std::wstring& item : chain) {
            if (item == name) {
                exists = true;
                break;
            }
        }

        if (!exists)
            chain.push_back(name);
    }

    return chain;
}

std::wstring pickFamilyForChar(const std::vector<std::wstring>& chain, wchar_t c) {
    if (chain.empty())
        return chooseFamily(L"Segoe UI", c);

    return chooseFamily(chain.front(), c);
}

const FontMetrics& getFontMetrics(const FontKey& key) {
    HFONT font = getFont(key); // takes lock itself (recursive)
    std::lock_guard<std::recursive_mutex> lock(g_mutex);

    auto it = g_metrics.find(key);
    if (it != g_metrics.end())
        return it->second;

    FontMetrics fm;
    fm.font = font;
    HDC dc = measureDc();
    HGDIOBJ old = SelectObject(dc, font);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    fm.height = tm.tmHeight;
    fm.ascent = tm.tmAscent;
    fm.descent = tm.tmDescent;
    fm.internalLeading = tm.tmInternalLeading;
    fm.externalLeading = tm.tmExternalLeading;
    return g_metrics.emplace(key, std::move(fm)).first->second;
}

int FontMetrics::advance(wchar_t c) const {
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    auto it = cache.find(c);
    if (it != cache.end())
        return it->second;

    HDC dc = measureDc();
    HGDIOBJ old = SelectObject(dc, font);
    int w = 0;
    if (!GetCharWidth32W(dc, c, c, &w)) {
        SIZE s{};
        GetTextExtentPoint32W(dc, &c, 1, &s);
        w = s.cx;
    }
    SelectObject(dc, old);
    cache[c] = w;
    return w;
}

void clearFontCache() {
    std::lock_guard<std::recursive_mutex> lock(g_mutex);

    for (auto& entry : g_fonts) {
        if (entry.second &&
            entry.second != static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)))
            DeleteObject(entry.second);
    }

    g_fonts.clear();
    g_metrics.clear();
}

} // namespace ui
