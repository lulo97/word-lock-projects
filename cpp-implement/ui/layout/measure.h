#pragma once
#include "dom/node.h"

namespace ui {

// Intrinsic sizes of an element.
struct Measure {
    float minContent = 0;   // narrowest without overflow (longest word)
    float maxContent = 0;   // widest (no wrapping)
};

// Measure intrinsic widths. In: element, dpiScale. Out: Measure (cached per layout pass).
Measure measureElement(Element& el, float scale);

// Height of an element given a definite width. In: el, width, scale. Out: px.
float measureHeightForWidth(Element& el, float width, float scale);

// Text extent of a string with a style. In: text, style, scale. Out: size (single line).
SizeF measureText(const std::wstring& text, const ComputedStyle& style, float scale);

// Clear the per-pass measure cache. Call at start of layout.
void clearMeasureCache();

} // namespace ui
