#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include "core/types.h"

namespace ui {

struct Path;          // svg/path.h
struct StrokeStyle;   // svg/svg_paint.h
struct TextRun;       // paint/text_draw.h

// Thin drawing wrapper over GDI+ (shapes) and GDI (text). Everything draws through this,
// so the rest of the code never touches GDI+ directly.
class Canvas {
public:
    // Create a canvas drawing into an HDC. In: hdc (usually a memory DC), width, height px.
    Canvas(HDC hdc, int width, int height);
    ~Canvas();

    // Fill a rounded rect. In: rect, radii, color (alpha respected).
    void fillRoundRect(const RectF& r, const CornerRadii& radii, Color c);

    // Stroke borders with per-side widths/colors. In: border-box rect, radii, widths, colors[4].
    void strokeBorders(const RectF& r, const CornerRadii& radii, const Edges& widths, const Color colors[4]);

    // Draw a line. In: p1, p2, width, color.
    void drawLine(PointF a, PointF b, float width, Color c);

    // Draw an ellipse (for slider thumb). In: bounding rect, fill, optional stroke.
    void fillEllipse(const RectF& r, Color c);

    // Clip to a rounded rect; nests. In: rect, radii. Pair with popClip().
    void pushClip(const RectF& r, const CornerRadii& radii);
    void popClip();

    // Stroke/fill a vector path with a transform. In: path, stroke, fill color, matrix (scale+translate).
    void drawPath(const Path& p, const StrokeStyle& stroke, Color fill, float scale, PointF translate);

    // Draw a text run at its x/y (delegates to GDI with cleartype). In: run.
    void drawText(const TextRun& run);

    // Raw HDC for GDI text and BitBlt. Out: HDC.
    HDC hdc() const;

    // Translate subsequent drawing (used for scroll containers). Nests.
    void pushTranslate(float dx, float dy);
    void popTranslate();

    // Set global opacity multiplier for subsequent draws. In: 0..1.
    void setOpacity(float a);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

// Start/stop GDI+ once per process. Call from window host init/shutdown.
bool initGraphics();
void shutdownGraphics();

} // namespace ui
