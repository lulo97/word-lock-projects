#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

namespace ui {

struct FontKey {
    std::wstring family;
    int px = 16;
    int weight = 400;
    bool italic = false;
    bool operator==(const FontKey& o) const;
    bool operator<(const FontKey& o) const;
};

struct FontMetrics {
    HFONT font = nullptr;
    int height = 0, ascent = 0, descent = 0, internalLeading = 0, externalLeading = 0;
    int advance(wchar_t c) const;                    // cached integer advance
    mutable std::unordered_map<wchar_t, int> cache;
};

// Get (or create) a cached HFONT. In: key. Out: HFONT owned by the cache (do not delete).
HFONT getFont(const FontKey& key);
// Get font + GDI metrics + cached glyph advances. Out: metrics owned by cache.
const FontMetrics& getFontMetrics(const FontKey& key);

// Build a fallback chain for a font family, e.g. "Segoe UI" -> {Segoe UI, Yu Gothic UI, Meiryo}.
std::vector<std::wstring> fallbackChain(const std::wstring& family);

// Pick the family from a chain that has the glyph for a code point. Out: family name.
std::wstring pickFamilyForChar(const std::vector<std::wstring>& chain, wchar_t c);

// Release all cached fonts. Call at shutdown or when DPI changes.
void clearFontCache();

} // namespace ui
