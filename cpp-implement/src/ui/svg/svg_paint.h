#pragma once
#include "paint/canvas.h"
#include "svg/svg_parse.h"

namespace ui {

struct StrokeStyle {
    float width = 1;
    Color color;
    LineCap cap = LineCap::Butt;
    LineJoin join = LineJoin::Miter;
    bool enabled = false;
};

// Paint an SvgDoc into a target rect with xMidYMid meet scaling.
// In: canvas, doc, content rect, currentColor (inherited text color). Out: pixels.
void paintSvg(Canvas& canvas, const SvgDoc& doc, const RectF& target, Color currentColor);

} // namespace ui
