#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "core/types.h"
#include "style/style.h"

namespace ui {
class Canvas;

// A piece of text with a single style, ready to measure/draw.
struct TextRun {
    std::wstring text;
    std::wstring family;
    float fontPx = 16;
    int weight = 400;
    bool italic = false;
    // Legacy; family is resolved once in makeRun(). Kept for compatibility.
    bool forceCjk = false;
    bool hanging = false;   // closing punctuation allowed to hang past line end (kinsoku)
    float letterSpacing = 0;
    Color color;
    float x = 0, y = 0;       // top-left after layout
    float width = 0, height = 0;
    float lineHeightPx = 0;    // resolved CSS line-height in physical px
    // Real GDI font metrics (physical px), filled by measureRun().
    float ascent = 0, descent = 0, internalLeading = 0, externalLeading = 0;
};

inline int snapPx(float v) { return static_cast<int>(std::lround(v)); }

// Build a TextRun from text and computed style (applies text-transform). Out: run.
TextRun makeRun(const std::wstring& text, const ComputedStyle& style, float scale);

// Measure run width with letter-spacing and font fallback. In/Out: run.width/height set.
void measureRun(TextRun& run);

// Draw a run (splits by script for fallback, char-by-char when letter-spacing != 0).
// In: canvas, run. Out: pixels drawn at run.x/run.y.
void drawRun(Canvas& canvas, const TextRun& run);

// Sub-run with same font/style as src but different text (measured).
TextRun sliceRun(const TextRun& src, const std::wstring& text);

// Per-character advance widths. In: run. Out: vector of cumulative x offsets (size n+1). Used by caret hit-testing.
std::vector<float> charOffsets(const TextRun& run);

} // namespace ui
