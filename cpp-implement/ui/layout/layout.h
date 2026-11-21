#pragma once
#include "dom/node.h"

namespace ui {

// Constraints given to an element during layout.
struct Constraints {
    float availWidth = 0;
    float availHeight = 0;
    bool  definiteWidth = false;    // true if width is forced by the parent
    bool  definiteHeight = false;
    float viewportWidth = 0, viewportHeight = 0;   // for min-h-screen
};

// Lay out the entire document into the viewport.
// In: root (styles resolved), viewport size, dpi scale. Out: every Element.box filled.
void layoutDocument(Element& root, float viewportW, float viewportH, float scale);

// Lay out one element and its subtree at (x,y).
// In: el, x, y, constraints, scale. Out: el.box set; returns outer height incl. margins.
float layoutElement(Element& el, float x, float y, const Constraints& c, float scale);

// Block flow: stack children vertically, apply space-y, collapse nothing (Tailwind uses no margin collapse in flex).
// Returns content height.
float layoutBlockChildren(Element& el, float contentX, float contentY, float innerW, float scale);

// Resolve final width. In: style width/min/max, constraints. Out: px (border-box).
float resolveWidth(Element& el, const Constraints& c, float scale);

// Resolve final height after content known. In: el, contentHeight, constraints. Out: px (border-box).
float resolveHeight(Element& el, float contentHeight, const Constraints& c, float scale);

} // namespace ui
