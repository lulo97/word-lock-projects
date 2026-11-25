#include "paint/paint.h"

#include "controls/control.h"
#include "dom/tree_ops.h"
#include "layout/inline_text.h"
#include "layout/layout_units.h"
#include "layout/position.h"
#include "paint/scrollbar.h"
#include "paint/shadow.h"
#include "paint/text_draw.h"
#include "svg/svg_parse.h"
#include "svg/svg_paint.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>

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
    auto hasLayer = [](const char* name) {
        const char* env = std::getenv("KOTOBA_OVERLAY");
        if (!env || !*env) return true; // default: all basic layers
        std::string s(env);
        return s.find(name) != std::string::npos || s.find("all") != std::string::npos;
    };
    const bool showBoxes = hasLayer("boxes");
    const bool showContent = hasLayer("content");
    const bool showLines = hasLayer("lines");
    const bool showRuns = hasLayer("runs");
    const bool showBaseline = hasLayer("baseline");

    std::function<void(Element&)> drawEl;
    drawEl = [&](Element& el) {
        RectF r = el.box.borderRect;
        if (showBoxes) {
            Color color = el.style.display == Display::None
                ? Color::rgba(255, 0, 0, 100)
                : Color::rgba(0, 180, 255, 180);
            Edges widths { 1, 1, 1, 1 };
            Color colors[4] = { color, color, color, color };
            canvas.strokeBorders(r, {}, widths, colors);
        }

        if (showContent) {
            RectF c = el.box.contentRect();
            Color cc = Color::rgba(120, 120, 120, 120);
            Edges cw{1,1,1,1}; Color ccs[4]={cc,cc,cc,cc};
            canvas.strokeBorders(c, {}, cw, ccs);
        }

        const auto& lbs = getLines(el);
        if (!lbs.empty()) {
            const RectF contentR = el.box.contentRect();
            const bool clipY = el.style.overflowY != Overflow::Visible;
            const bool clipX = el.style.overflowX != Overflow::Visible;
            for (const LineBox& ln : lbs) {
                if (showLines) {
                    RectF lr{contentR.x, ln.y, std::max(0.f, ln.width), std::max(0.f, ln.height)};
                    Color lc = Color::rgba(0, 200, 0, 160);
                    Edges lw{1,1,1,1}; Color lcs[4]={lc,lc,lc,lc};
                    canvas.strokeBorders(lr, {}, lw, lcs);
                }
                for (const TextRun& tr : ln.runs) {
                    const float emTop = tr.y + tr.internalLeading;
                    const float emBottom = tr.y + tr.height;
                    const float right = tr.x + tr.width;
                    bool overT = !clipY && toUnits(emTop) < toUnits(contentR.y);
                    bool overB = !clipY && toUnits(emBottom) > toUnits(contentR.bottom());
                    bool overR = !clipX && !tr.hanging && toUnits(right) > toUnits(contentR.right());
                    bool over = overT || overB || overR || toUnits(tr.fontPx) > toUnits(ln.height);
                    if (showRuns) {
                        RectF rr{tr.x, tr.y, tr.width, tr.height};
                        Color rc = over ? Color::rgba(255, 0, 0, 220) : Color::rgba(220, 0, 220, 160);
                        Edges rw{1,1,1,1}; Color rcs[4]={rc,rc,rc,rc};
                        canvas.strokeBorders(rr, {}, rw, rcs);
                    }
                    if (showBaseline) {
                        float by = tr.y + tr.ascent;
                        Color bc = Color::rgba(255, 180, 0, 200);
                        canvas.drawLine({tr.x, by}, {tr.x + tr.width, by}, 1.f, bc);
                    }
                }
            }
        }

        if (&el == hovered) {
            Color hot = Color::rgba(255, 220, 0, 220);
            Edges hotWidths { 2, 2, 2, 2 };
            Color hotColors[4] = { hot, hot, hot, hot };
            canvas.strokeBorders(r, {}, hotWidths, hotColors);
        }

        const bool clipped = el.style.overflowX != Overflow::Visible ||
                             el.style.overflowY != Overflow::Visible;
        if (clipped) {
            canvas.pushClip(r, {});
            canvas.pushTranslate(-el.box.scrollX, -el.box.scrollY);
        }
        for (auto& ch : el.children)
            if (ch->kind == Node::Kind::Element)
                drawEl(*static_cast<Element*>(ch.get()));
        if (clipped) {
            canvas.popTranslate();
            canvas.popClip();
        }
    };

    drawEl(root);
}

} // namespace ui
