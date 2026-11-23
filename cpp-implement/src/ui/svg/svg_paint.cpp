#include "svg/svg_paint.h"

#include <algorithm>

namespace ui {

void paintSvg(
    Canvas& canvas,
    const SvgDoc& doc,
    const RectF& target,
    Color currentColor
) {
    if (doc.items.empty() || doc.vbW <= 0 || doc.vbH <= 0)
        return;

    float scale = std::min(target.w / doc.vbW, target.h / doc.vbH);
    float offsetX = target.x + (target.w - doc.vbW * scale) / 2 - doc.vbX * scale;
    float offsetY = target.y + (target.h - doc.vbH * scale) / 2 - doc.vbY * scale;

    for (const SvgDoc::Item& item : doc.items) {
        StrokeStyle stroke;
        stroke.enabled = item.hasStroke;
        stroke.width = item.strokeWidth * scale;
        stroke.color = item.strokeCurrentColor ? currentColor : item.stroke;
        stroke.cap = item.cap;
        stroke.join = item.join;

        Color fill = Color::transparent();

        if (item.hasFill)
            fill = item.fillCurrentColor ? currentColor : item.fill;

        canvas.drawPath(
            item.path,
            stroke,
            fill,
            scale,
            { offsetX, offsetY }
        );
    }
}

} // namespace ui
