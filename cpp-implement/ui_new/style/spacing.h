#pragma once
#include <string>
#include "core/types.h"

namespace ui {

// Tailwind spacing scale: "4" -> 16px, "2.5" -> 10px, "px" -> 1px. In: key. Out: true + px.
bool parseSpacingScale(const std::wstring& key, float& px);

// Parse arbitrary value inside [...]: "13px", "0.15em", "1.5rem", "50%".
// In: inner text, emBasis (font size for em). Out: true + px (percent returned via isPercent).
bool parseArbitraryLength(const std::wstring& inner, float emBasis, float& value, bool& isPercent);

// Parse a fraction like "1/2", "2/3". In: text. Out: true + percent (0..100).
bool parseFraction(const std::wstring& key, float& percent);

// Convert a width/height value token (scale, fraction, full, screen, [..]) into a Length.
// In: key (text after "w-"). Out: true + length.
bool parseSizeToken(const std::wstring& key, Length& out);

} // namespace ui
