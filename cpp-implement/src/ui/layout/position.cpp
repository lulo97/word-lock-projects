#include "layout/position.h"

#include "layout/inline_text.h"
#include "layout/layout.h"
#include "layout/measure.h"

#include <algorithm>

namespace ui {

namespace {

float resolveLength(const Length& value, float basis, float fallback, float scale) {
    switch (value.unit) {
    case Length::Unit::Px:
        return value.value * scale;
    case Length::Unit::Percent:
    case Length::Unit::Screen:
        return basis * value.value / 100.0f;
    case Length::Unit::Auto:
    default:
        return fallback;
    }
}

float horizontalBox(const ComputedStyle& style, float scale) {
    return (style.padding.left + style.padding.right +
            style.borderWidth.left + style.borderWidth.right) * scale;
}

} // namespace

Element* findContainingBlock(Element& el) {
    Node* current = el.parent;

    while (current) {
        Element* element = dynamic_cast<Element*>(current);

        if (!element)
            break;

        if (element->style.position != Position::Static)
            return element;

        current = current->parent;
    }

    return nullptr;
}

void layoutAbsolute(Element& el, const RectF& containingRect, float scale) {
    const ComputedStyle& style = el.style;

    float marginLeft = style.margin.left * scale;
    float marginRight = style.margin.right * scale;
    float marginTop = style.margin.top * scale;
    float marginBottom = style.margin.bottom * scale;

    bool hasLeft = !style.left.isAuto();
    bool hasRight = !style.right.isAuto();
    bool hasTop = !style.top.isAuto();
    bool hasBottom = !style.bottom.isAuto();

    float left = hasLeft
        ? resolveLength(style.left, containingRect.w, 0, scale) : 0;
    float right = hasRight
        ? resolveLength(style.right, containingRect.w, 0, scale) : 0;
    float top = hasTop
        ? resolveLength(style.top, containingRect.h, 0, scale) : 0;
    float bottom = hasBottom
        ? resolveLength(style.bottom, containingRect.h, 0, scale) : 0;

    float width = 0;

    if (style.width.isAuto() && hasLeft && hasRight) {
        width = std::max(0.f, containingRect.w - left - right -
                             marginLeft - marginRight);
    }
    else if (!style.width.isAuto()) {
        Constraints c;
        c.availWidth = containingRect.w;
        c.availHeight = containingRect.h;
        c.definiteWidth = false;
        c.definiteHeight = false;
        c.viewportWidth = containingRect.w;
        c.viewportHeight = containingRect.h;
        width = resolveWidth(el, c, scale);
    }
    else {
        Measure intrinsic = measureElement(el, scale);
        width = intrinsic.maxContent + horizontalBox(style, scale);
    }

    float x = containingRect.x;

    if (hasLeft)
        x += left + marginLeft;
    else if (hasRight)
        x += containingRect.w - right - width - marginRight;

    float y = containingRect.y;

    // With only `bottom` set, the element's height is not known until after
    // layout, so position it provisionally at the containing block's top and
    // align its bottom edge once the box has been laid out.
    const bool bottomOnly = hasBottom && !hasTop;

    if (hasTop)
        y += top + marginTop;

    Constraints c;
    c.availWidth = width;
    c.availHeight = containingRect.h;
    c.definiteWidth = true;
    c.definiteHeight = false;
    c.viewportWidth = containingRect.w;
    c.viewportHeight = containingRect.h;

    layoutElement(el, x, y, c, scale);

    if (bottomOnly) {
        float targetBottom = containingRect.y + containingRect.h - bottom - marginBottom;
        float alignDy = targetBottom - el.box.borderRect.bottom();

        if (alignDy != 0)
            translateSubtree(el, 0, alignDy);
    }

    float dx = el.box.borderRect.w * style.translateXPct / 100.0f;
    float dy = el.box.borderRect.h * style.translateYPct / 100.0f;

    if (dx != 0 || dy != 0)
        translateSubtree(el, dx, dy);
}

void applyRelativeOffset(Element& el, float scale) {
    float dx = 0;
    float dy = 0;

    if (!el.style.left.isAuto())
        dx += resolveLength(el.style.left, el.box.borderRect.w, 0, scale);
    else if (!el.style.right.isAuto())
        dx -= resolveLength(el.style.right, el.box.borderRect.w, 0, scale);

    if (!el.style.top.isAuto())
        dy += resolveLength(el.style.top, el.box.borderRect.h, 0, scale);
    else if (!el.style.bottom.isAuto())
        dy -= resolveLength(el.style.bottom, el.box.borderRect.h, 0, scale);

    dx += el.box.borderRect.w * el.style.translateXPct / 100.0f;
    dy += el.box.borderRect.h * el.style.translateYPct / 100.0f;

    if (dx != 0 || dy != 0)
        translateSubtree(el, dx, dy);
}

std::vector<Element*> paintOrderChildren(Element& el) {
    struct Item {
        Element* element;
        int z;
        size_t order;
    };

    std::vector<Item> items;
    size_t order = 0;

    for (auto& childPtr : el.children) {
        Element* child = dynamic_cast<Element*>(childPtr.get());

        if (!child || child->style.display == Display::None)
            continue;

        items.push_back({ child, child->style.zIndex, order++ });
    }

    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.z != b.z)
            return a.z < b.z;

        return a.order < b.order;
    });

    std::vector<Element*> out;
    out.reserve(items.size());

    for (const Item& item : items)
        out.push_back(item.element);

    return out;
}

void translateSubtree(Element& el, float dx, float dy) {
    el.box.borderRect.x += dx;
    el.box.borderRect.y += dy;
    translateLines(el, dx, dy);

    for (auto& childPtr : el.children) {
        Element* child = dynamic_cast<Element*>(childPtr.get());

        if (child)
            translateSubtree(*child, dx, dy);
    }
}

} // namespace ui
