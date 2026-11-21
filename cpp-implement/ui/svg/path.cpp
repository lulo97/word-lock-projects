#include "svg/path.h"

#include "core/utf.h"

#include <cmath>
#include <cwctype>
#include <vector>

namespace ui {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct PathParser {
    const std::wstring& data;
    size_t pos = 0;

    void skip() {
        while (pos < data.size()) {
            wchar_t c = data[pos];

            if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L',')
                ++pos;
            else
                break;
        }
    }

    bool number(float& out) {
        skip();

        if (pos >= data.size())
            return false;

        size_t start = pos;

        if (data[pos] == L'+' || data[pos] == L'-')
            ++pos;

        bool digits = false;

        while (pos < data.size() && iswdigit(data[pos])) {
            digits = true;
            ++pos;
        }

        if (pos < data.size() && data[pos] == L'.') {
            ++pos;

            while (pos < data.size() && iswdigit(data[pos])) {
                digits = true;
                ++pos;
            }
        }

        if (!digits)
            return false;

        if (pos < data.size() && (data[pos] == L'e' || data[pos] == L'E')) {
            size_t exponent = pos++;
            if (pos < data.size() && (data[pos] == L'+' || data[pos] == L'-'))
                ++pos;

            bool expDigits = false;

            while (pos < data.size() && iswdigit(data[pos])) {
                expDigits = true;
                ++pos;
            }

            if (!expDigits)
                pos = exponent;
        }

        return parseFloat(data.substr(start, pos - start), out);
    }

    bool flag(bool& out) {
        skip();

        if (pos >= data.size())
            return false;

        if (data[pos] == L'0') { out = false; ++pos; return true; }
        if (data[pos] == L'1') { out = true; ++pos; return true; }
        return false;
    }

    wchar_t command() {
        skip();

        if (pos >= data.size())
            return 0;

        wchar_t c = data[pos];

        if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z')) {
            ++pos;
            return c;
        }

        return 0;
    }
};

PointF add(PointF a, PointF b) {
    return { a.x + b.x, a.y + b.y };
}

} // namespace

void arcToCubics(
    PointF start,
    float rx,
    float ry,
    float rotDeg,
    bool largeArc,
    bool sweep,
    PointF end,
    std::vector<PathSeg>& out
) {
    if (rx == 0 || ry == 0) {
        PathSeg line;
        line.kind = SegKind::LineTo;
        line.p1 = end;
        out.push_back(line);
        return;
    }

    rx = std::fabs(rx);
    ry = std::fabs(ry);

    if (start.x == end.x && start.y == end.y)
        return;

    float phi = rotDeg * kPi / 180.0f;
    float cosPhi = std::cos(phi);
    float sinPhi = std::sin(phi);

    float dx = (start.x - end.x) / 2.0f;
    float dy = (start.y - end.y) / 2.0f;

    float x1p = cosPhi * dx + sinPhi * dy;
    float y1p = -sinPhi * dx + cosPhi * dy;

    float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);

    if (lambda > 1) {
        float scale = std::sqrt(lambda);
        rx *= scale;
        ry *= scale;
    }

    float rx2 = rx * rx;
    float ry2 = ry * ry;
    float x1p2 = x1p * x1p;
    float y1p2 = y1p * y1p;

    float denominator = rx2 * y1p2 + ry2 * x1p2;
    float numerator = std::max(0.f, rx2 * ry2 - denominator);
    float factor = denominator == 0 ? 0 : std::sqrt(numerator / denominator);

    if (largeArc == sweep)
        factor = -factor;

    float cxp = factor * (rx * y1p / ry);
    float cyp = factor * (-ry * x1p / rx);

    float cx = cosPhi * cxp - sinPhi * cyp + (start.x + end.x) / 2.0f;
    float cy = sinPhi * cxp + cosPhi * cyp + (start.y + end.y) / 2.0f;

    auto angle = [](float ux, float uy, float vx, float vy) {
        float dot = ux * vx + uy * vy;
        float len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));

        if (len == 0)
            return 0.f;

        float a = std::acos(std::max(-1.f, std::min(1.f, dot / len)));
        float cross = ux * vy - uy * vx;

        return cross < 0 ? -a : a;
    };

    float ux = (x1p - cxp) / rx;
    float uy = (y1p - cyp) / ry;
    float vx = (-x1p - cxp) / rx;
    float vy = (-y1p - cyp) / ry;

    float theta1 = angle(1, 0, ux, uy);
    float delta = angle(ux, uy, vx, vy);

    if (!sweep && delta > 0)
        delta -= 2 * kPi;
    else if (sweep && delta < 0)
        delta += 2 * kPi;

    int segments = std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / (kPi / 2))));
    float segmentAngle = delta / segments;
    float alpha = 4.0f / 3.0f * std::tan(segmentAngle / 4.0f);

    float theta = theta1;

    auto pointOnArc = [&](float t) {
        float cosT = std::cos(t);
        float sinT = std::sin(t);
        float x = rx * cosT;
        float y = ry * sinT;
        return PointF {
            cx + cosPhi * x - sinPhi * y,
            cy + sinPhi * x + cosPhi * y
        };
    };

    for (int i = 0; i < segments; ++i) {
        float theta2 = theta + segmentAngle;

        PointF p1 = pointOnArc(theta);
        PointF p2 = pointOnArc(theta2);

        float cos1 = std::cos(theta);
        float sin1 = std::sin(theta);
        float cos2 = std::cos(theta2);
        float sin2 = std::sin(theta2);

        PointF c1 {
            cx + cosPhi * (rx * (cos1 - alpha * sin1)) - sinPhi * (ry * (sin1 + alpha * cos1)),
            cy + sinPhi * (rx * (cos1 - alpha * sin1)) + cosPhi * (ry * (sin1 + alpha * cos1))
        };

        PointF c2 {
            cx + cosPhi * (rx * (cos2 + alpha * sin2)) - sinPhi * (ry * (sin2 - alpha * cos2)),
            cy + sinPhi * (rx * (cos2 + alpha * sin2)) + cosPhi * (ry * (sin2 - alpha * cos2))
        };

        PathSeg seg;
        seg.kind = SegKind::CubicTo;
        seg.p1 = c1;
        seg.p2 = c2;
        seg.p3 = p2;
        out.push_back(seg);

        (void)p1;
        theta = theta2;
    }
}

bool parsePathData(const std::wstring& d, Path& out) {
    out.segs.clear();

    PathParser parser{ d };
    wchar_t command = 0;
    wchar_t previousCommand = 0;

    PointF current { 0, 0 };
    PointF start { 0, 0 };
    PointF lastCubicControl { 0, 0 };
    PointF lastQuadControl { 0, 0 };

    while (true) {
        parser.skip();

        if (parser.pos >= d.size())
            break;

        wchar_t next = parser.command();

        if (next)
            command = next;
        else if (!command)
            break;

        bool relative = iswlower(command);
        wchar_t cmd = static_cast<wchar_t>(towupper(command));
        bool repeatCommand = next != 0;
        (void)repeatCommand;

        if (cmd == L'Z') {
            PathSeg seg;
            seg.kind = SegKind::Close;
            out.segs.push_back(seg);
            current = start;
            previousCommand = command;
            command = 0;
            continue;
        }

        auto point = [&](float x, float y) {
            return relative ? PointF { current.x + x, current.y + y }
                            : PointF { x, y };
        };

        if (cmd == L'M') {
            float x = 0, y = 0;

            if (!parser.number(x) || !parser.number(y))
                break;

            current = point(x, y);
            start = current;
            PathSeg seg;
            seg.kind = SegKind::MoveTo;
            seg.p1 = current;
            out.segs.push_back(seg);

            command = relative ? L'l' : L'L';
            previousCommand = L'M';
            continue;
        }

        if (cmd == L'L') {
            float x = 0, y = 0;

            if (!parser.number(x) || !parser.number(y))
                break;

            current = point(x, y);
            PathSeg seg;
            seg.kind = SegKind::LineTo;
            seg.p1 = current;
            out.segs.push_back(seg);
            previousCommand = command;
            continue;
        }

        if (cmd == L'H') {
            float x = 0;

            if (!parser.number(x))
                break;

            current.x = relative ? current.x + x : x;
            PathSeg seg;
            seg.kind = SegKind::LineTo;
            seg.p1 = current;
            out.segs.push_back(seg);
            previousCommand = command;
            continue;
        }

        if (cmd == L'V') {
            float y = 0;

            if (!parser.number(y))
                break;

            current.y = relative ? current.y + y : y;
            PathSeg seg;
            seg.kind = SegKind::LineTo;
            seg.p1 = current;
            out.segs.push_back(seg);
            previousCommand = command;
            continue;
        }

        if (cmd == L'C') {
            float x1, y1, x2, y2, x, y;

            if (!parser.number(x1) || !parser.number(y1) ||
                !parser.number(x2) || !parser.number(y2) ||
                !parser.number(x) || !parser.number(y))
                break;

            PointF c1 = point(x1, y1);
            PointF c2 = point(x2, y2);
            PointF end = point(x, y);

            PathSeg seg;
            seg.kind = SegKind::CubicTo;
            seg.p1 = c1;
            seg.p2 = c2;
            seg.p3 = end;
            out.segs.push_back(seg);

            lastCubicControl = c2;
            current = end;
            previousCommand = command;
            continue;
        }

        if (cmd == L'S') {
            float x2, y2, x, y;

            if (!parser.number(x2) || !parser.number(y2) ||
                !parser.number(x) || !parser.number(y))
                break;

            PointF c1 = current;

            if (previousCommand == L'C' || previousCommand == L'c' ||
                previousCommand == L'S' || previousCommand == L's') {
                c1 = {
                    2 * current.x - lastCubicControl.x,
                    2 * current.y - lastCubicControl.y
                };
            }

            PointF c2 = point(x2, y2);
            PointF end = point(x, y);

            PathSeg seg;
            seg.kind = SegKind::CubicTo;
            seg.p1 = c1;
            seg.p2 = c2;
            seg.p3 = end;
            out.segs.push_back(seg);

            lastCubicControl = c2;
            current = end;
            previousCommand = command;
            continue;
        }

        if (cmd == L'Q' || cmd == L'T') {
            PointF q;
            PointF end;

            if (cmd == L'Q') {
                float x1, y1, x, y;

                if (!parser.number(x1) || !parser.number(y1) ||
                    !parser.number(x) || !parser.number(y))
                    break;

                q = point(x1, y1);
                end = point(x, y);
                lastQuadControl = q;
            }
            else {
                float x, y;

                if (!parser.number(x) || !parser.number(y))
                    break;

                if (previousCommand == L'Q' || previousCommand == L'q' ||
                    previousCommand == L'T' || previousCommand == L't') {
                    q = {
                        2 * current.x - lastQuadControl.x,
                        2 * current.y - lastQuadControl.y
                    };
                }
                else {
                    q = current;
                }

                end = point(x, y);
                lastQuadControl = q;
            }

            PointF c1 {
                current.x + 2.0f / 3.0f * (q.x - current.x),
                current.y + 2.0f / 3.0f * (q.y - current.y)
            };
            PointF c2 {
                end.x + 2.0f / 3.0f * (q.x - end.x),
                end.y + 2.0f / 3.0f * (q.y - end.y)
            };

            PathSeg seg;
            seg.kind = SegKind::CubicTo;
            seg.p1 = c1;
            seg.p2 = c2;
            seg.p3 = end;
            out.segs.push_back(seg);

            current = end;
            previousCommand = command;
            continue;
        }

        if (cmd == L'A') {
            float rx, ry, rot, x, y;
            bool largeArc = false;
            bool sweep = false;

            if (!parser.number(rx) || !parser.number(ry) ||
                !parser.number(rot) || !parser.flag(largeArc) ||
                !parser.flag(sweep) || !parser.number(x) ||
                !parser.number(y))
                break;

            PointF end = point(x, y);
            arcToCubics(current, rx, ry, rot, largeArc, sweep, end, out.segs);
            current = end;
            previousCommand = command;
            continue;
        }

        break;
    }

    return !out.segs.empty();
}

} // namespace ui
