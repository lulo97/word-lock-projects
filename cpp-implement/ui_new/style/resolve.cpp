#include "style/resolve.h"

#include "core/utf.h"
#include "dom/tree_ops.h"
#include "style/resolve.h"
#include "style/variants.h"

#include <cstring>

namespace ui {

namespace {

bool lengthEqual(const Length& a, const Length& b) {
    return a.unit == b.unit && a.value == b.value;
}

bool edgesEqual(const Edges& a, const Edges& b) {
    return a.top == b.top && a.right == b.right &&
           a.bottom == b.bottom && a.left == b.left;
}

void applyDeclarations(ComputedStyle& style, const Declarations& decls) {
    for (const Declaration& d : decls)
        applyDeclaration(style, d);
}

bool isHeading(const std::wstring& tag) {
    return tag == L"h1" || tag == L"h2" || tag == L"h3" ||
           tag == L"h4" || tag == L"h5" || tag == L"h6";
}

} // namespace

void applyPreflight(ComputedStyle& style, const std::wstring& tag) {
    style.borderColor[0] = Color::rgb(229, 231, 235);
    style.borderColor[1] = Color::rgb(229, 231, 235);
    style.borderColor[2] = Color::rgb(229, 231, 235);
    style.borderColor[3] = Color::rgb(229, 231, 235);

    style.margin = {};
    style.padding = {};
    style.borderWidth = {};
    style.radius = {};
    style.background = Color::transparent();

    if (tag == L"root" || tag == L"html" || tag == L"body" ||
        tag == L"div" || tag == L"p" || tag == L"span" ||
        tag == L"label" || isHeading(tag) || tag == L"template") {
        style.margin = {};
    }

    if (tag == L"body") {
        style.fontFamily = L"Segoe UI";
        style.fontSize = 16;
        style.lineHeight = 1.5f;
        style.lineHeightIsPx = false;
        style.color = Color::rgb(0, 0, 0);
        style.display = Display::Block;
    }

    if (isHeading(tag)) {
        style.fontWeight = 700;
    }

    if (tag == L"button") {
        style.background = Color::transparent();
        style.color = Color::rgb(0, 0, 0);
        style.padding = {};
        style.borderWidth = {};
        style.radius = {};
        style.display = Display::InlineBlock;
        style.fontSize = 16;
        style.fontWeight = 400;
    }

    if (tag == L"input" || tag == L"textarea" || tag == L"select") {
        style.background = Color::transparent();
        style.padding = {};
        style.borderWidth = {};
        style.radius = {};
        style.display = Display::InlineBlock;
    }

    if (tag == L"svg") {
        style.display = Display::InlineBlock;
    }

    if (tag == L"span" || tag == L"label" || tag == L"a" ||
        tag == L"strong" || tag == L"em" || tag == L"b" ||
        tag == L"i" || tag == L"small") {
        style.display = Display::Inline;
    }

    if (tag == L"template" || tag == L"head" || tag == L"script" ||
        tag == L"style" || tag == L"title") {
        style.display = Display::None;
    }
}

void inheritFrom(const ComputedStyle& parent, ComputedStyle& child) {
    child.color = parent.color;
    child.fontSize = parent.fontSize;
    child.fontWeight = parent.fontWeight;
    child.italic = parent.italic;
    child.fontFamily = parent.fontFamily;
    child.letterSpacing = parent.letterSpacing;
    child.lineHeight = parent.lineHeight;
    child.lineHeightIsPx = parent.lineHeightIsPx;
    child.textAlign = parent.textAlign;
    child.transform = parent.transform;
    child.cursor = parent.cursor;
}

void resolveElement(Element& el, const ComputedStyle* parentStyle, const ResolveContext& ctx) {
    ComputedStyle previous = el.style;
    ComputedStyle style;

    applyPreflight(style, el.tag);

    if (parentStyle)
        inheritFrom(*parentStyle, style);

    if (el.hasAttr(L"hidden") || el.hasAttr(L"data-ui-hidden"))
        style.display = Display::None;

    VariantSet classes = buildVariantSet(el.classList);

    applyDeclarations(style, classes.base);

    if (isVariantActive(Variant::Sm, el.stateFlags, ctx.viewportWidth))
        applyDeclarations(style, classes.sm);

    if (isVariantActive(Variant::Md, el.stateFlags, ctx.viewportWidth))
        applyDeclarations(style, classes.md);

    if (isVariantActive(Variant::Lg, el.stateFlags, ctx.viewportWidth))
        applyDeclarations(style, classes.lg);

    if (el.control) {
        // Inputs keep their transparent Tailwind background unless a bg-*
        // class set one. Control text follows the inherited color.
    }

    if (isVariantActive(Variant::Hover, el.stateFlags, ctx.viewportWidth))
        applyDeclarations(style, classes.hover);

    if (isVariantActive(Variant::Focus, el.stateFlags, ctx.viewportWidth))
        applyDeclarations(style, classes.focus);

    if (isVariantActive(Variant::Active, el.stateFlags, ctx.viewportWidth))
        applyDeclarations(style, classes.active);

    if (isVariantActive(Variant::Disabled, el.stateFlags, ctx.viewportWidth))
        applyDeclarations(style, classes.disabled);

    if (el.hasAttr(L"autofocus") && !el.stateFlags)
        el.stateFlags |= StateFocus;

    el.style = style;

    if (el.styleDirty || styleAffectsLayout(previous, style))
        el.layoutDirty = true;

    el.styleDirty = false;
}

void resolveTree(Element& root, const ResolveContext& ctx, bool force) {
    resolveElement(root, nullptr, ctx);

    std::vector<Element*> children;

    for (auto& child : root.children) {
        Element* element = dynamic_cast<Element*>(child.get());
        if (element)
            children.push_back(element);
    }

    for (Element* child : children)
        resolveTree(*child, ctx, force);
}

bool styleAffectsLayout(const ComputedStyle& a, const ComputedStyle& b) {
    return a.display != b.display ||
           a.position != b.position ||
           !lengthEqual(a.left, b.left) ||
           !lengthEqual(a.right, b.right) ||
           !lengthEqual(a.top, b.top) ||
           !lengthEqual(a.bottom, b.bottom) ||
           a.translateXPct != b.translateXPct ||
           a.translateYPct != b.translateYPct ||
           !lengthEqual(a.width, b.width) ||
           !lengthEqual(a.height, b.height) ||
           !lengthEqual(a.minWidth, b.minWidth) ||
           !lengthEqual(a.minHeight, b.minHeight) ||
           !lengthEqual(a.maxWidth, b.maxWidth) ||
           !lengthEqual(a.maxHeight, b.maxHeight) ||
           !edgesEqual(a.margin, b.margin) ||
           !edgesEqual(a.padding, b.padding) ||
           !edgesEqual(a.borderWidth, b.borderWidth) ||
           a.spaceX != b.spaceX || a.spaceY != b.spaceY ||
           a.dir != b.dir || a.justify != b.justify || a.align != b.align ||
           a.grow != b.grow || a.shrink != b.shrink ||
           a.gapX != b.gapX || a.gapY != b.gapY ||
           a.overflowX != b.overflowX || a.overflowY != b.overflowY ||
           a.fontSize != b.fontSize || a.fontWeight != b.fontWeight ||
           a.italic != b.italic || a.fontFamily != b.fontFamily ||
           a.letterSpacing != b.letterSpacing ||
           a.lineHeight != b.lineHeight ||
           a.lineHeightIsPx != b.lineHeightIsPx ||
           a.textAlign != b.textAlign || a.transform != b.transform;
}

} // namespace ui
