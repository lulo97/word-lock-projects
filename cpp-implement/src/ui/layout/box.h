#pragma once
#include "core/types.h"

namespace ui {

// Result of layout for one element. All coordinates are absolute in document space
// (before scroll offset), in physical pixels.
struct LayoutBox {
    RectF borderRect;       // border-box rect (outer edge of border)
    Edges margin;           // resolved margins
    Edges border;           // resolved border widths
    Edges padding;          // resolved padding
    float contentHeight = 0;// full scrollable content height (overflow containers)
    float scrollX = 0, scrollY = 0;   // current scroll offsets

    // Padding-box rect (inside border).
    RectF paddingRect() const;
    // Content-box rect (inside padding).
    RectF contentRect() const;
    // Client height visible inside the box (padding box height).
    float clientHeight() const;
    // Max scrollY value. Out: >= 0.
    float maxScrollY() const;
};

} // namespace ui
