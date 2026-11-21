#pragma once
#include <string>
#include <vector>
#include "core/types.h"

namespace ui {

enum class SegKind { MoveTo, LineTo, CubicTo, Close };

// One normalized path segment (absolute coordinates, cubic Beziers only).
struct PathSeg {
    SegKind kind;
    PointF p1, p2, p3;   // LineTo/MoveTo use p1; CubicTo uses p1,p2 (controls), p3 (end)
};

struct Path { std::vector<PathSeg> segs; };

// Parse SVG path data. Supports M m L l H h V v C c S s Q q A a Z z.
// In: d attribute. Out: true + normalized path (all absolute, arcs converted to cubics).
bool parsePathData(const std::wstring& d, Path& out);

// Convert an SVG arc to 1..4 cubic Beziers.
// In: start, rx, ry, xAxisRotation(deg), largeArc, sweep, end. Out: appended segs.
void arcToCubics(PointF start, float rx, float ry, float rotDeg, bool largeArc, bool sweep,
                 PointF end, std::vector<PathSeg>& out);

} // namespace ui
