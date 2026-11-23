#pragma once
#include "dom/node.h"

namespace ui {

// Cascade options for one resolve pass.
struct ResolveContext {
    int viewportWidth = 0;      // for sm:/md:/lg:
    int viewportHeight = 0;     // for h-screen
};

// Apply preflight defaults for a tag (box-sizing, heading reset, button reset, etc).
// In/Out: style. In: tag.
void applyPreflight(ComputedStyle& style, const std::wstring& tag);

// Copy inherited properties (color, font*, letter-spacing, line-height, align, transform)
// from parent style. In: parent, out child style.
void inheritFrom(const ComputedStyle& parent, ComputedStyle& child);

// Resolve a single element: preflight -> inherit -> base classes -> active variants.
// Out: element.style filled; layoutDirty set if geometry-affecting fields changed.
void resolveElement(Element& el, const ComputedStyle* parentStyle, const ResolveContext& ctx);

// Resolve a whole tree top-down. In: root, ctx. Skips clean subtrees unless force=true.
void resolveTree(Element& root, const ResolveContext& ctx, bool force);

// Compare two styles for geometry-affecting differences. Out: true if relayout is needed.
bool styleAffectsLayout(const ComputedStyle& a, const ComputedStyle& b);

} // namespace ui
