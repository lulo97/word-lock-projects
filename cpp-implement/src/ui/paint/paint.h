#pragma once
#include "paint/canvas.h"
#include "dom/node.h"

namespace ui {

// Paint the whole document. In: canvas, root, scale. Out: pixels.
void paintDocument(Canvas& canvas, Element& root, float scale);

// Paint one element in order: shadow, background, border, clip, children, control, scrollbar.
void paintElement(Canvas& canvas, Element& el, float scale);

// Paint inline text lines of an element. In: canvas, element.
void paintInlineText(Canvas& canvas, Element& el);

// Debug: draw outline of every border box and the hovered element's info. In: canvas, root.
void paintDebugOverlay(Canvas& canvas, Element& root, const Element* hovered);

} // namespace ui
