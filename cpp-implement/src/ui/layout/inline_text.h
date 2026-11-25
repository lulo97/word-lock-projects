#pragma once
#include <vector>
#include "dom/node.h"
#include "paint/text_draw.h"

namespace ui {

// One laid-out line of inline content.
struct LineBox {
    std::vector<TextRun> runs;   // runs with absolute x positions after alignment
    float width = 0, height = 0, baseline = 0;
    float y = 0;
};

// Collect inline items from an element's children (text nodes, span, label) into styled runs.
// In: el, scale. Out: list of (text, style) items in document order.
std::vector<TextRun> collectInlineRuns(Element& el, float scale = 1.f);

struct InlineExtent { float minContent = 0, maxContent = 0; };
// Intrinsic widths computed by the SAME tokenizer/breaker that lays the text out.
InlineExtent measureInline(Element& el, float scale);

// Break runs into lines. Breaks at spaces, between CJK chars (respect isLineStartForbidden),
// and inside overlong words. In: runs, maxWidth, scale. Out: lines (positions relative to content box).
std::vector<LineBox> breakLines(const std::vector<TextRun>& runs, float maxWidth, float scale);

// Apply text-align and line-height to lines. In/Out: lines. In: style, contentWidth.
void alignLines(std::vector<LineBox>& lines, const ComputedStyle& style, float contentWidth);

// Lay out an element whose children are all inline. Stores lines in element for painting.
// Out: content height.
float layoutInline(Element& el, float contentX, float contentY, float innerW, float scale);

// Does this element establish inline content (p, h*, button, label, span w/ text)?
bool hasInlineContent(const Element& el);

// Fetch cached lines computed by layoutInline for painting. Out: lines or empty.
const std::vector<LineBox>& getLines(const Element& el);

// Shift cached line boxes (used when an element is moved after layout).
void translateLines(Element& el, float dx, float dy);

// Drop cached line boxes for a new layout pass.
void clearLineCache();
void eraseLines(const Element& el);
} // namespace ui
