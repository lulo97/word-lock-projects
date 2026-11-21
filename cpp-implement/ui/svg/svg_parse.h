#pragma once
#include <vector>
#include "svg/path.h"
#include "dom/node.h"
#include "core/types.h"

namespace ui {

enum class LineCap { Butt, Round, Square };
enum class LineJoin { Miter, Round, Bevel };

// Parsed <svg> element ready for painting.
struct SvgDoc {
    float vbX = 0, vbY = 0, vbW = 24, vbH = 24;   // viewBox
    struct Item {
        Path path;
        bool hasFill = false, hasStroke = false;
        bool fillCurrentColor = false, strokeCurrentColor = false;
        Color fill, stroke;
        float strokeWidth = 1;
        LineCap cap = LineCap::Butt;
        LineJoin join = LineJoin::Miter;
    };
    std::vector<Item> items;
};

// Parse an <svg> Element (and its <path> children) into an SvgDoc.
// In: svg element. Out: SvgDoc (attrs inherited from <svg> down to paths).
SvgDoc parseSvg(const Element& svgEl);

// Parse viewBox="x y w h". Out: true on success.
bool parseViewBox(const std::wstring& s, float& x, float& y, float& w, float& h);

} // namespace ui
