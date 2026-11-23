#pragma once
#include "layout/layout.h"

namespace ui {

// Flex layout for display:flex containers.
// In: container, content-box origin, inner width, optional fixed inner height (-1 if auto),
//     constraints, scale. Out: children boxes placed; returns resulting content height.
float layoutFlex(Element& container, float contentX, float contentY, float innerW,
                 float fixedInnerH, const Constraints& c, float scale);

// Row-specific: grow/shrink distribution, justify, align.
float layoutFlexRow(Element& container, float contentX, float contentY, float innerW,
                    float fixedInnerH, const Constraints& c, float scale);

// Column-specific: grow with definite height, justify, align.
float layoutFlexColumn(Element& container, float contentX, float contentY, float innerW,
                       float fixedInnerH, const Constraints& c, float scale);

// Compute leading offset and spacing for justify-content.
// In: mode, free space, item count. Out: startOffset, extraSpacing between items.
void distributeJustify(JustifyContent mode, float freeSpace, int count, float& startOffset, float& extraSpacing);

} // namespace ui
