#include "svg/svg_parse.h"

#include "core/utf.h"
#include "style/palette.h"

#include <cstdlib>

namespace ui {

namespace {

struct Inherited {
    bool hasFill = false;
    bool fillCurrent = false;
    Color fill = Color::rgb(0, 0, 0);
    bool hasStroke = false;
    bool strokeCurrent = false;
    Color stroke = Color::rgb(0, 0, 0);
    float strokeWidth = 1;
    LineCap cap = LineCap::Butt;
    LineJoin join = LineJoin::Miter;
};

LineCap parseCap(const std::wstring& value) {
    if (value == L"round") return LineCap::Round;
    if (value == L"square") return LineCap::Square;
    return LineCap::Butt;
}

LineJoin parseJoin(const std::wstring& value) {
    if (value == L"round") return LineJoin::Round;
    if (value == L"bevel") return LineJoin::Bevel;
    return LineJoin::Miter;
}

void parsePaint(const std::wstring& value, bool inheritedCurrent,
                bool& has, bool& current, Color& color) {
    if (value.empty()) {
        has = false;
        current = inheritedCurrent;
        return;
    }

    if (value == L"none") {
        has = false;
        current = false;
        return;
    }

    if (value == L"currentColor" || value == L"currentcolor") {
        has = true;
        current = true;
        return;
    }

    Color parsed;

    if (parseCssColor(value, parsed)) {
        has = true;
        current = false;
        color = parsed;
    }
}

Inherited inheritedFrom(const Element& el, const Inherited& parent) {
    Inherited out = parent;

    std::wstring fill = el.attr(L"fill");

    if (!fill.empty())
        parsePaint(fill, parent.fillCurrent, out.hasFill, out.fillCurrent, out.fill);

    std::wstring stroke = el.attr(L"stroke");

    if (!stroke.empty())
        parsePaint(stroke, parent.strokeCurrent, out.hasStroke, out.strokeCurrent, out.stroke);

    std::wstring width = el.attr(L"stroke-width");
    float widthValue = 0;

    if (!width.empty() && parseFloat(width, widthValue))
        out.strokeWidth = widthValue;

    std::wstring cap = el.attr(L"stroke-linecap");

    if (!cap.empty())
        out.cap = parseCap(cap);

    std::wstring join = el.attr(L"stroke-linejoin");

    if (!join.empty())
        out.join = parseJoin(join);

    return out;
}

void collectPaths(const Element& el, const Inherited& inherited, SvgDoc& doc) {
    Inherited own = inheritedFrom(el, inherited);

    if (el.tag == L"path") {
        Path path;

        if (parsePathData(el.attr(L"d"), path)) {
            SvgDoc::Item item;
            item.path = std::move(path);
            item.hasFill = own.hasFill;
            item.fillCurrentColor = own.fillCurrent;
            item.fill = own.fill;
            item.hasStroke = own.hasStroke;
            item.strokeCurrentColor = own.strokeCurrent;
            item.stroke = own.stroke;
            item.strokeWidth = own.strokeWidth;
            item.cap = own.cap;
            item.join = own.join;
            doc.items.push_back(std::move(item));
        }
    }

    for (const auto& childPtr : el.children) {
        if (childPtr->kind == Node::Kind::Element)
            collectPaths(*static_cast<const Element*>(childPtr.get()), own, doc);
    }
}

} // namespace

bool parseViewBox(const std::wstring& input, float& x, float& y, float& w, float& h) {
    std::vector<std::wstring> parts;

    for (const std::wstring& token : splitWhitespace(input))
        parts.push_back(token);

    if (parts.size() != 4)
        return false;

    return parseFloat(parts[0], x) && parseFloat(parts[1], y) &&
           parseFloat(parts[2], w) && parseFloat(parts[3], h);
}

SvgDoc parseSvg(const Element& svgEl) {
    SvgDoc doc;

    if (!parseViewBox(svgEl.attr(L"viewbox"), doc.vbX, doc.vbY, doc.vbW, doc.vbH)) {
        doc.vbX = 0;
        doc.vbY = 0;
        doc.vbW = 24;
        doc.vbH = 24;
    }

    Inherited root;
    root.hasFill = false; // <svg fill="none"> is the common case
    root.hasStroke = false;
    root.stroke = Color::rgb(0, 0, 0);
    root.fill = Color::rgb(0, 0, 0);
    root = inheritedFrom(svgEl, root);

    for (const auto& childPtr : svgEl.children) {
        if (childPtr->kind == Node::Kind::Element)
            collectPaths(*static_cast<const Element*>(childPtr.get()), root, doc);
    }

    return doc;
}

} // namespace ui
