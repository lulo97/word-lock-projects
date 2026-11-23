#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <string>
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

// Get (or create) a cached HFONT. In: key. Out: HFONT owned by the cache (do not delete).
HFONT getFont(const FontKey& key);

// Build a fallback chain for a font family, e.g. "Segoe UI" -> {Segoe UI, Yu Gothic UI, Meiryo}.
std::vector<std::wstring> fallbackChain(const std::wstring& family);

// Pick the family from a chain that has the glyph for a code point. Out: family name.
std::wstring pickFamilyForChar(const std::vector<std::wstring>& chain, wchar_t c);

// Release all cached fonts. Call at shutdown or when DPI changes.
void clearFontCache();

} // namespace ui
