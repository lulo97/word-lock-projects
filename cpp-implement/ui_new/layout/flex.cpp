#include "layout/flex.h"

#include "layout/inline_text.h"
#include "layout/measure.h"
#include "layout/position.h"

#include <algorithm>
#include <vector>

namespace ui {

namespace {

float marginH(const Element& el, float scale) {
    return (el.style.margin.left + el.style.margin.right) * scale;
}

float marginV(const Element& el, float scale) {
    return (el.style.margin.top + el.style.margin.bottom) * scale;
}

float boxH(const Element& el, float scale) {
    return (el.style.padding.left + el.style.padding.right +
            el.style.borderWidth.left + el.style.borderWidth.right) * scale;
}

float boxV(const Element& el, float scale) {
    return (el.style.padding.top + el.style.padding.bottom +
            el.style.borderWidth.top + el.style.borderWidth.bottom) * scale;
}

std::vector<Element*> flexChildren(Element& container) {
    std::vector<Element*> out;

    for (auto& childPtr : container.children) {
        Element* child = dynamic_cast<Element*>(childPtr.get());

        if (!child || child->style.display == Display::None ||
            child->style.position == Position::Absolute)
            continue;

        out.push_back(child);
    }

    return out;
}

float baseWidth(Element& child, float available, const Constraints& input, float scale) {
    if (child.style.grow > 0 && child.style.width.isAuto())
        return 0;

    Constraints c = input;
    c.availWidth = std::max(0.f, available - marginH(child, scale));
    c.definiteWidth = false;
    c.definiteHeight = false;

    float width = resolveWidth(child, c, scale);

    if (child.style.width.isAuto()) {
        Measure intrinsic = measureElement(child, scale);
        width = intrinsic.maxContent + boxH(child, scale);
        width = std::min(width, std::max(0.f, available - marginH(child, scale)));

        if (!child.style.maxWidth.isAuto() &&
            child.style.maxWidth.unit == Length::Unit::Px) {
            width = std::min(width, child.style.maxWidth.value * scale);
        }
    }

    return width;
}

} // namespace

void distributeJustify(
    JustifyContent mode,
    float freeSpace,
    int count,
    float& startOffset,
    float& extraSpacing
) {
    startOffset = 0;
    extraSpacing = 0;

    if (count <= 0 || freeSpace <= 0)
        return;

    switch (mode) {
    case JustifyContent::Center:
        startOffset = freeSpace / 2;
        break;
    case JustifyContent::End:
        startOffset = freeSpace;
        break;
    case JustifyContent::Between:
        if (count > 1)
            extraSpacing = freeSpace / (count - 1);
        break;
    case JustifyContent::Around:
        extraSpacing = freeSpace / count;
        startOffset = extraSpacing / 2;
        break;
    case JustifyContent::Evenly:
        extraSpacing = freeSpace / (count + 1);
        startOffset = extraSpacing;
        break;
    case JustifyContent::Start:
    default:
        break;
    }
}

float layoutFlexRow(
    Element& container,
    float contentX,
    float contentY,
    float innerW,
    float fixedInnerH,
    const Constraints& input,
    float scale
) {
    std::vector<Element*> children = flexChildren(container);
    const int count = static_cast<int>(children.size());

    if (count == 0)
        return fixedInnerH >= 0 ? fixedInnerH : 0;

    const float gap = container.style.gapX * scale + container.style.spaceX * scale;

    std::vector<float> widths(count, 0);
    std::vector<float> margins(count, 0);
    float used = 0;
    int growCount = 0;

    for (int i = 0; i < count; ++i) {
        Element& child = *children[i];
        margins[i] = marginH(child, scale);
        widths[i] = baseWidth(child, innerW, input, scale);
        used += widths[i] + margins[i];

        if (child.style.grow > 0)
            ++growCount;
    }

    used += gap * std::max(0, count - 1);

    float freeSpace = innerW - used;

    if (growCount > 0 && freeSpace > 0) {
        float each = freeSpace / growCount;

        for (int i = 0; i < count; ++i) {
            if (children[i]->style.grow > 0)
                widths[i] += each * children[i]->style.grow;
        }
    }
    else if (freeSpace < 0) {
        // Distribute shrink proportionally, never below zero.
        float shrinkable = 0;

        for (int i = 0; i < count; ++i) {
            if (children[i]->style.shrink > 0)
                shrinkable += widths[i];
        }

        if (shrinkable > 0) {
            float ratio = std::min(1.f, -freeSpace / shrinkable);

            for (int i = 0; i < count; ++i) {
                if (children[i]->style.shrink > 0)
                    widths[i] -= widths[i] * ratio;
            }

            freeSpace = 0;
        }
    }

    float startOffset = 0;
    float extraSpacing = 0;
    float finalUsed = 0;

    for (int i = 0; i < count; ++i)
        finalUsed += widths[i] + margins[i];

    finalUsed += gap * std::max(0, count - 1);

    distributeJustify(
        container.style.justify,
        std::max(0.f, innerW - finalUsed),
        count,
        startOffset,
        extraSpacing
    );

    float cursor = contentX + startOffset;

    std::vector<float> childHeights(count, 0);
    float maxOuterH = 0;

    for (int pass = 0; pass < 2; ++pass) {
        cursor = contentX + startOffset;

        for (int i = 0; i < count; ++i) {
            Element& child = *children[i];
            float mx = child.style.margin.left * scale;
            float my = child.style.margin.top * scale;
            float mb = child.style.margin.bottom * scale;

            Constraints c = input;
            c.availWidth = std::max(0.f, widths[i]);
            c.definiteWidth = true;
            c.availHeight = fixedInnerH >= 0 ? std::max(0.f, fixedInnerH - my - mb)
                                             : input.availHeight;
            c.definiteHeight = fixedInnerH >= 0 && container.style.align == AlignItems::Stretch;

            if (pass == 0) {
                layoutElement(child, cursor + mx, contentY + my, c, scale);
                childHeights[i] = child.box.borderRect.h;
                maxOuterH = std::max(maxOuterH, childHeights[i] + my + mb);
            }
            else {
                float availableH = fixedInnerH >= 0 ? fixedInnerH : maxOuterH;
                float offset = my;

                if (container.style.align == AlignItems::Center)
                    offset = (availableH - childHeights[i]) / 2;
                else if (container.style.align == AlignItems::End)
                    offset = availableH - childHeights[i] - mb;

                offset = std::max(0.f, offset);

                layoutElement(child, cursor + mx, contentY + offset, c, scale);
            }

            cursor += widths[i] + mx + (child.style.margin.right * scale) +
                      gap + extraSpacing;
        }
    }

    return fixedInnerH >= 0 ? fixedInnerH : maxOuterH;
}

float layoutFlexColumn(
    Element& container,
    float contentX,
    float contentY,
    float innerW,
    float fixedInnerH,
    const Constraints& input,
    float scale
) {
    std::vector<Element*> children = flexChildren(container);
    const int count = static_cast<int>(children.size());

    if (count == 0)
        return fixedInnerH >= 0 ? fixedInnerH : 0;

    const float gap = container.style.gapY * scale + container.style.spaceY * scale;

    std::vector<float> widths(count, 0);
    std::vector<float> heights(count, 0);
    std::vector<float> marginTop(count, 0);
    std::vector<float> marginBottom(count, 0);

    int growCount = 0;
    float nonGrowHeight = 0;

    for (int i = 0; i < count; ++i) {
        Element& child = *children[i];

        float mx = marginH(child, scale);
        float availableWidth = std::max(0.f, innerW - mx);

        if (container.style.align == AlignItems::Stretch ||
            (!child.style.width.isAuto())) {
            Constraints c = input;
            c.availWidth = availableWidth;
            c.definiteWidth = container.style.align == AlignItems::Stretch;
            widths[i] = resolveWidth(child, c, scale);

            if (child.style.width.isAuto() && container.style.align != AlignItems::Stretch) {
                Measure intrinsic = measureElement(child, scale);
                widths[i] = std::min(availableWidth, intrinsic.maxContent + boxH(child, scale));
            }
        }
        else {
            Measure intrinsic = measureElement(child, scale);
            widths[i] = std::min(availableWidth, intrinsic.maxContent + boxH(child, scale));
        }

        marginTop[i] = child.style.margin.top * scale;
        marginBottom[i] = child.style.margin.bottom * scale;

        if (child.style.grow > 0) {
            ++growCount;
            continue;
        }

        float explicitHeight = -1;

        if (!child.style.height.isAuto()) {
            Constraints hc = input;
            hc.availHeight = fixedInnerH >= 0 ? fixedInnerH : input.availHeight;
            explicitHeight = resolveHeight(child, 0, hc, scale);
        }

        if (explicitHeight >= 0) {
            heights[i] = explicitHeight;
        }
        else {
            Constraints c = input;
            c.availWidth = widths[i];
            c.availHeight = fixedInnerH >= 0 ? fixedInnerH : input.availHeight;
            c.definiteWidth = true;
            c.definiteHeight = false;

            layoutElement(child, contentX, contentY, c, scale);
            heights[i] = child.box.borderRect.h;
        }

        nonGrowHeight += heights[i] + marginTop[i] + marginBottom[i];
    }

    float gaps = gap * std::max(0, count - 1);
    float targetHeight = fixedInnerH >= 0
        ? fixedInnerH
        : nonGrowHeight + gaps;

    float freeSpace = targetHeight - nonGrowHeight - gaps;
    float growEach = growCount > 0 ? std::max(0.f, freeSpace) / growCount : 0;

    if (growCount > 0 && fixedInnerH < 0) {
        growEach = 0;
        targetHeight = nonGrowHeight + gaps;
    }

    for (int i = 0; i < count; ++i) {
        if (children[i]->style.grow <= 0)
            continue;

        heights[i] = growEach * children[i]->style.grow;
        targetHeight = std::max(targetHeight,
            nonGrowHeight + gaps + growEach * growCount);
    }

    float finalUsed = 0;

    for (int i = 0; i < count; ++i)
        finalUsed += heights[i] + marginTop[i] + marginBottom[i];

    finalUsed += gaps;

    float startOffset = 0;
    float extraSpacing = 0;

    distributeJustify(
        container.style.justify,
        std::max(0.f, targetHeight - finalUsed),
        count,
        startOffset,
        extraSpacing
    );

    float cursor = contentY + startOffset;

    for (int i = 0; i < count; ++i) {
        Element& child = *children[i];

        float mx = child.style.margin.left * scale;
        float x = contentX + mx;
        float availableWidth = std::max(0.f, innerW - marginH(child, scale));
        float childWidth = widths[i];

        if (container.style.align == AlignItems::Center)
            x = contentX + (innerW - childWidth) / 2;
        else if (container.style.align == AlignItems::End)
            x = contentX + innerW - childWidth - (child.style.margin.right * scale);

        cursor += marginTop[i];

        Constraints c = input;
        c.availWidth = childWidth;
        c.definiteWidth = true;
        c.availHeight = heights[i] > 0 ? heights[i] : input.availHeight;
        c.definiteHeight = heights[i] > 0;

        layoutElement(child, x, cursor, c, scale);

        cursor = child.box.borderRect.bottom() + marginBottom[i] +
                 gap + extraSpacing;
    }

    float actualBottom = 0;

    for (int i = 0; i < count; ++i) {
        Element& child = *children[i];
        actualBottom = std::max(
            actualBottom,
            child.box.borderRect.bottom() + marginBottom[i] - contentY
        );
    }

    if (fixedInnerH >= 0)
        return std::max(targetHeight, actualBottom);

    return std::max(actualBottom, targetHeight);
}

float layoutFlex(
    Element& container,
    float contentX,
    float contentY,
    float innerW,
    float fixedInnerH,
    const Constraints& c,
    float scale
) {
    if (container.style.dir == FlexDirection::Column) {
        return layoutFlexColumn(
            container, contentX, contentY, innerW,
            fixedInnerH, c, scale
        );
    }

    return layoutFlexRow(
        container, contentX, contentY, innerW,
        fixedInnerH, c, scale
    );
}

} // namespace ui
