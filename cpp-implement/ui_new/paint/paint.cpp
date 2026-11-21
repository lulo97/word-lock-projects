#include "paint/paint.h"

#include "controls/control.h"
#include "dom/tree_ops.h"
#include "layout/inline_text.h"
#include "layout/position.h"
#include "paint/scrollbar.h"
#include "paint/shadow.h"
#include "paint/text_draw.h"
#include "svg/svg_parse.h"
#include "svg/svg_paint.h"

#include <algorithm>

namespace ui {

namespace {

CornerRadii scaledRadii(const CornerRadii& radii, float scale) {
    return {
        radii.tl * scale,
        radii.tr * scale,
        radii.br * scale,
        radii.bl * scale
    };
}

Edges scaledEdges(const Edges& edges, float scale) {
    return {
        edges.top * scale,
        edges.right * scale,
        edges.bottom * scale,
        edges.left * scale
    };
}

} // namespace

void paintDocument(Canvas& canvas, Element& root, float scale) {
    paintElement(canvas, root, scale);
}

void paintInlineText(Canvas& canvas, Element& el) {
    for (const LineBox& line : getLines(el)) {
        for (const TextRun& run : line.runs)
            canvas.drawText(run);
    }
}

void paintElement(Canvas& canvas, Element& el, float scale) {
    if (el.style.display == Display::None)
        return;

    RectF rect = el.box.borderRect;

    if (rect.w <= 0 || rect.h <= 0)
        return;

    CornerRadii radii = scaledRadii(el.style.radius, scale);

    for (const Shadow& shadow : el.style.shadows)
        drawShadow(canvas, rect, radii, shadow, scale);

    if (el.style.background.a > 0)
        canvas.fillRoundRect(rect, radii, el.style.background);

    Edges borders = scaledEdges(el.style.borderWidth, scale);
    if (borders.top > 0 || borders.right > 0 ||
        borders.bottom > 0 || borders.left > 0) {
        canvas.strokeBorders(rect, radii, borders, el.style.borderColor);
    }

    bool clipped = el.style.overflowX != Overflow::Visible ||
                   el.style.overflowY != Overflow::Visible;

    if (clipped) {
        RectF clip = el.box.paddingRect();
        canvas.pushClip(clip, radii);
    }

    if (el.tag == L"svg") {
        SvgDoc doc = parseSvg(el);
        paintSvg(canvas, doc, el.box.contentRect(), el.style.color);
    }
    else {
        bool scrolled = el.box.scrollX != 0 || el.box.scrollY != 0;

        if (scrolled)
            canvas.pushTranslate(-el.box.scrollX, -el.box.scrollY);

        paintInlineText(canvas, el);

        for (Element* child : paintOrderChildren(el))
            paintElement(canvas, *child, scale);

        if (scrolled)
            canvas.popTranslate();
    }

    if (el.control)
        el.control->paint(canvas, el, scale);

    if (needsScrollbar(el))
        drawScrollbar(canvas, el, scale);

    if (clipped)
        canvas.popClip();
}

void paintDebugOverlay(Canvas& canvas, Element& root, const Element* hovered) {
    auto outline = [&](Element& el) {
        Color color = el.style.display == Display::None
            ? Color::rgba(255, 0, 0, 100)
            : Color::rgba(0, 180, 255, 180);

        RectF r = el.box.borderRect;
        Edges widths { 1, 1, 1, 1 };
        Color colors[4] = { color, color, color, color };
        canvas.strokeBorders(r, {}, widths, colors);

        if (&el == hovered) {
            Color hot = Color::rgba(255, 220, 0, 220);
            Edges hotWidths { 2, 2, 2, 2 };
            Color hotColors[4] = { hot, hot, hot, hot };
            canvas.strokeBorders(r, {}, hotWidths, hotColors);
        }
    };

    walk(root, [&](Element& el) {
        outline(el);
        return true;
    });
}

} // namespace ui
