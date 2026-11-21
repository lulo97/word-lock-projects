#pragma once
#include "dom/node.h"

namespace ui {

// Find the nearest ancestor with position != static. Out: element or nullptr (viewport).
Element* findContainingBlock(Element& el);

// Place an absolutely positioned element.
// In: el, containing block rect, scale. Out: el.box.borderRect set (after left/top/right/bottom + translate).
void layoutAbsolute(Element& el, const RectF& containingRect, float scale);

// Apply position:relative offsets (left/top) after normal layout. In/Out: el.
void applyRelativeOffset(Element& el, float scale);

// Collect children in paint order (z-index, then document order). In: el. Out: sorted pointers.
std::vector<Element*> paintOrderChildren(Element& el);

// Shift an element subtree by dx,dy (used after flex alignment / scroll). In/Out: el.
void translateSubtree(Element& el, float dx, float dy);

} // namespace ui
