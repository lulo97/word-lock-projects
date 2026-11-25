#pragma once
#include "paint/canvas.h"
#include "dom/node.h"

namespace ui {

constexpr float kScrollbarWidth = 15.f;

// Draw the thin dark overlay scrollbar for an overflow-y-auto element.
// In: canvas, element (box.scrollY/contentHeight used), scale.
void drawScrollbar(Canvas& canvas, const Element& el, float scale);

// Thumb rect for hit testing and drag. Out: rect in document coords (empty if no scrollbar).
RectF scrollbarThumbRect(const Element& el, float scale);

// Track rect excluding arrow buttons. Out: rect in document coords (empty if no scrollbar).
RectF scrollbarTrackRect(const Element& el, float scale);

// Does this element need a scrollbar (overflowY auto and content taller than client)?
bool needsScrollbar(const Element& el);

} // namespace ui
