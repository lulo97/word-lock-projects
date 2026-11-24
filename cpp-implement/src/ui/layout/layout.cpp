#include "layout/layout.h"

#include "dom/tree_ops.h"
#include "layout/flex.h"
#include "layout/inline_text.h"
#include "layout/measure.h"
#include "layout/position.h"
#include "style/style.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

float g_viewportW = 0;
float g_viewportH = 0;

float resolveLength(const Length& value, float basis, float fallback, float scale) {
    switch (value.unit) {
    case Length::Unit::Px:
        return value.value * scale;
    case Length::Unit::Percent:
        return basis * value.value / 100.0f;
    case Length::Unit::Screen:
        return basis * value.value / 100.0f;
    case Length::Unit::Auto:
    default:
        return fallback;
    }
}

Edges scaledEdges(const Edges& edges, float scale) {
    return {
        edges.top * scale,
        edges.right * scale,
        edges.bottom * scale,
        edges.left * scale
    };
}

float verticalBox(const ComputedStyle& style, float scale) {
    return (style.padding.top + style.padding.bottom +
            style.borderWidth.top + style.borderWidth.bottom) * scale;
}

float horizontalBox(const ComputedStyle& style, float scale) {
    return (style.padding.left + style.padding.right +
            style.borderWidth.left + style.borderWidth.right) * scale;
}

} // namespace

float resolveWidth(Element& el, const Constraints& c, float scale) {
    const ComputedStyle& style = el.style;
    float available = std::max(0.f, c.availWidth);
    float width = available;

    // A parent (flex, absolute positioning) already resolved this width.
    // Never apply a percentage/fraction a second time.
    if (!c.definiteWidth) {
        if (!style.width.isAuto()) {
            float basis = style.width.unit == Length::Unit::Screen
                ? c.viewportWidth : available;
            width = resolveLength(style.width, basis, available, scale);
        }
        else if (style.display == Display::Inline ||
                 style.display == Display::InlineBlock) {
            Measure intrinsic = measureElement(el, scale);
            width = intrinsic.maxContent + horizontalBox(style, scale);
            width = std::min(width, available);
        }
    }

    if (!style.minWidth.isAuto()) {
        float minWidth = resolveLength(
            style.minWidth,
            style.minWidth.unit == Length::Unit::Screen ? c.viewportWidth : available,
            0,
            scale
        );
        width = std::max(width, minWidth);
    }

    if (!style.maxWidth.isAuto()) {
        float maxWidth = resolveLength(
            style.maxWidth,
            style.maxWidth.unit == Length::Unit::Screen ? c.viewportWidth : available,
            available,
            scale
        );
        width = std::min(width, maxWidth);
    }

    return std::max(0.f, width);
}

float resolveHeight(Element& el, float contentHeight, const Constraints& c, float scale) {
    const ComputedStyle& style = el.style;
    float boxHeight;

    if (!style.height.isAuto()) {
        float basis = style.height.unit == Length::Unit::Screen
            ? c.viewportHeight : c.availHeight;
        boxHeight = resolveLength(style.height, basis, c.availHeight, scale);
    }
    else if (c.definiteHeight) {
        boxHeight = c.availHeight;
    }
    else {
        boxHeight = contentHeight + verticalBox(style, scale);
    }

    if (!style.minHeight.isAuto()) {
        float minHeight = resolveLength(
            style.minHeight,
            style.minHeight.unit == Length::Unit::Screen ? c.viewportHeight : c.availHeight,
            0,
            scale
        );
        boxHeight = std::max(boxHeight, minHeight);
    }

    if (!style.maxHeight.isAuto()) {
        float maxHeight = resolveLength(
            style.maxHeight,
            style.maxHeight.unit == Length::Unit::Screen ? c.viewportHeight : c.availHeight,
            c.availHeight,
            scale
        );
        boxHeight = std::min(boxHeight, maxHeight);
    }

    return std::max(0.f, boxHeight);
}

float layoutBlockChildren(
    Element& el,
    float contentX,
    float contentY,
    float innerW,
    float scale
) {
    Constraints c;
    c.availWidth = innerW;
    c.availHeight = 100000.f;
    c.viewportWidth = g_viewportW;
    c.viewportHeight = g_viewportH;

    float y = contentY;
    bool first = true;

    for (auto& childPtr : el.children) {
        Element* child = dynamic_cast<Element*>(childPtr.get());

        if (!child || child->style.display == Display::None ||
            child->style.position == Position::Absolute)
            continue;

        float marginTop = child->style.margin.top * scale;
        float marginBottom = child->style.margin.bottom * scale;
        float marginLeft = child->style.margin.left * scale;
        float marginRight = child->style.margin.right * scale;

        if (!first)
            y += el.style.spaceY * scale;

        y += marginTop;

        Constraints childC = c;
        childC.availWidth = std::max(0.f, innerW - marginLeft - marginRight);
        childC.availHeight = std::max(0.f, c.availHeight - (y - contentY));

        layoutElement(*child, contentX + marginLeft, y, childC, scale);

        y = child->box.borderRect.bottom() + marginBottom;
        first = false;
    }

    return std::max(0.f, y - contentY);
}

float layoutElement(Element& el, float x, float y, const Constraints& input, float scale) {
    eraseLines(el);   // drop this element's old text lines; layoutInline re-adds them
    const ComputedStyle& style = el.style;

    // Remember the scroll position: resetting the box must not reset scrolling.
    const float keepScrollX = el.box.scrollX;
    const float keepScrollY = el.box.scrollY;

    el.box = LayoutBox{};
    el.box.scrollX = keepScrollX;
    el.box.scrollY = keepScrollY;
    el.box.margin = scaledEdges(style.margin, scale);
    el.box.border = scaledEdges(style.borderWidth, scale);
    el.box.padding = scaledEdges(style.padding, scale);

    if (style.display == Display::None) {
        return 0;
    }

    float width = resolveWidth(el, input, scale);

    float explicitHeight = -1;

    if (!style.height.isAuto()) {
        float basis = style.height.unit == Length::Unit::Screen
            ? input.viewportHeight : input.availHeight;
        explicitHeight = resolveLength(style.height, basis, input.availHeight, scale);
    }
    else if (input.definiteHeight) {
        explicitHeight = input.availHeight;
    }

    float borderH = el.box.border.top + el.box.border.bottom;
    float paddingH = el.box.padding.top + el.box.padding.bottom;
    float borderW = el.box.border.left + el.box.border.right;
    float paddingW = el.box.padding.left + el.box.padding.right;

    float innerW = std::max(0.f, width - borderW - paddingW);
    float fixedContentH = explicitHeight >= 0
        ? std::max(0.f, explicitHeight - borderH - paddingH)
        : -1.f;

    float minOuterH = 0;

    if (!style.minHeight.isAuto()) {
        minOuterH = resolveLength(
            style.minHeight,
            style.minHeight.unit == Length::Unit::Screen
                ? input.viewportHeight : input.availHeight,
            0,
            scale
        );
    }

    if (fixedContentH < 0 && minOuterH > 0)
        fixedContentH = std::max(0.f, minOuterH - borderH - paddingH);

    float contentX = x + el.box.border.left + el.box.padding.left;
    float contentY = y + el.box.border.top + el.box.padding.top;

    float contentHeight = 0;

    if (style.display == Display::Flex) {
        contentHeight = layoutFlex(
            el, contentX, contentY, innerW,
            fixedContentH, input, scale
        );
    }
    else if (el.tag == L"svg") {
        contentHeight = fixedContentH >= 0
            ? fixedContentH
            : std::max(0.f, style.height.isAuto() ? 0.f : 0.f);
    }
    else if (hasInlineContent(el)) {
        contentHeight = layoutInline(el, contentX, contentY, innerW, scale);
    }
    else if (el.control) {
        SizeF intrinsic = el.control->intrinsicSize(el, scale);
        contentHeight = fixedContentH >= 0
            ? fixedContentH
            : std::max(0.f, intrinsic.h);
    }
    else {
        contentHeight = layoutBlockChildren(
            el, contentX, contentY, innerW, scale
        );
    }

    float boxHeight = resolveHeight(el, contentHeight, input, scale);

    el.box.borderRect = { x, y, width, boxHeight };
    el.box.contentHeight = contentHeight + paddingH;
    el.box.scrollX = std::min(el.box.scrollX, std::max(0.f, el.box.contentHeight));
    el.box.scrollY = std::min(el.box.scrollY, el.box.maxScrollY());

    return boxHeight + el.box.margin.top + el.box.margin.bottom;
}

void layoutDocument(Element& root, float viewportW, float viewportH, float scale) {
    clearMeasureCache();
    g_viewportW = viewportW;
    g_viewportH = viewportH;

    Constraints c;
    c.availWidth = viewportW;
    c.availHeight = viewportH;
    c.definiteWidth = false;
    c.definiteHeight = false;
    c.viewportWidth = viewportW;
    c.viewportHeight = viewportH;

    layoutElement(root, 0, 0, c, scale);

    std::vector<Element*> relatives;

    walk(root, [&](Element& el) {
        if (el.style.position == Position::Relative)
            relatives.push_back(&el);

        return true;
    });

    for (Element* element : relatives)
        applyRelativeOffset(*element, scale);

    std::vector<Element*> absolutes;

    walk(root, [&](Element& el) {
        if (el.style.position == Position::Absolute)
            absolutes.push_back(&el);

        return true;
    });

    for (Element* element : absolutes) {
        Element* containing = findContainingBlock(*element);
        RectF containingRect;

        if (containing)
            containingRect = containing->box.paddingRect();
        else
            containingRect = { 0, 0, viewportW, viewportH };

        layoutAbsolute(*element, containingRect, scale);
    }
}

} // namespace ui
