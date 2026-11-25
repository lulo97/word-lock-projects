#include "paint/scrollbar.h"

#include "svg/path.h"
#include "svg/svg_paint.h"

#include <algorithm>

namespace ui {

namespace {

constexpr float kArrowHFactor = 1.0f; // arrow button height = width

float arrowH(const Element& /*el*/, float scale) {
    return kScrollbarWidth * kArrowHFactor * scale;
}

bool hasArrows(const Element& el, float scale) {
    RectF client = el.box.paddingRect();
    return client.h >= arrowH(el, scale) * 2.f + 8.f * scale;
}

void fillTriangle(Canvas& canvas, PointF a, PointF b, PointF c, Color color) {
    Path p;
    p.segs.push_back({SegKind::MoveTo, a, {}, {}});
    p.segs.push_back({SegKind::LineTo, b, {}, {}});
    p.segs.push_back({SegKind::LineTo, c, {}, {}});
    p.segs.push_back({SegKind::Close, {}, {}, {}});
    StrokeStyle stroke; // enabled=false by default
    canvas.drawPath(p, stroke, color, 1.f, {0.f, 0.f});
}

} // namespace

bool needsScrollbar(const Element& el) {
    return el.style.overflowY == Overflow::Auto &&
           el.box.maxScrollY() > 0.5f;
}

RectF scrollbarTrackRect(const Element& el, float scale) {
    if (!needsScrollbar(el))
        return {};

    RectF client = el.box.paddingRect();
    float width = kScrollbarWidth * scale;
    float ah = hasArrows(el, scale) ? arrowH(el, scale) : 0.f;
    float x = client.right() - width;
    float y = client.y + ah;
    float h = std::max(0.f, client.h - ah * 2.f);
    return {x, y, width, h};
}

RectF scrollbarThumbRect(const Element& el, float scale) {
    if (!needsScrollbar(el))
        return {};

    RectF track = scrollbarTrackRect(el, scale);
    if (track.h <= 0.f)
        return {};

    RectF client = el.box.paddingRect();
    float contentH = std::max(el.box.contentHeight, client.h);
    float thumbH = std::max(20.f * scale, track.h * client.h / contentH);
    thumbH = std::min(track.h, thumbH);

    float maxScroll = el.box.maxScrollY();
    float progress = maxScroll > 0
        ? std::max(0.f, std::min(1.f, el.box.scrollY / maxScroll))
        : 0.f;

    float y = track.y + (track.h - thumbH) * progress;
    return {track.x, y, track.w, thumbH};
}

void drawScrollbar(Canvas& canvas, const Element& el, float scale) {
    if (!needsScrollbar(el))
        return;

    RectF client = el.box.paddingRect();
    float width = kScrollbarWidth * scale;
    float x = client.right() - width;
    float ah = hasArrows(el, scale) ? arrowH(el, scale) : 0.f;

    // Track (Chrome classic light gray)
    CornerRadii square{};
    canvas.fillRoundRect({x, client.y, width, client.h}, square, Color::rgb(0xf1, 0xf1, 0xf1));

    // Arrow buttons
    if (ah > 0.f) {
        Color arrowColor = Color::rgb(0x50, 0x50, 0x50);
        float cx = x + width / 2.f;
        float triW = 4.f * scale;
        float triH = 3.5f * scale;

        // Top arrow
        canvas.fillRoundRect({x, client.y, width, ah}, square, Color::rgb(0xf1, 0xf1, 0xf1));
        float topCy = client.y + ah / 2.f;
        fillTriangle(canvas,
            {cx, topCy - triH}, {cx - triW, topCy + triH * 0.6f}, {cx + triW, topCy + triH * 0.6f},
            arrowColor);

        // Bottom arrow
        float by = client.bottom() - ah;
        canvas.fillRoundRect({x, by, width, ah}, square, Color::rgb(0xf1, 0xf1, 0xf1));
        float botCy = by + ah / 2.f;
        fillTriangle(canvas,
            {cx - triW, botCy - triH * 0.6f}, {cx + triW, botCy - triH * 0.6f}, {cx, botCy + triH},
            arrowColor);
    }

    // Thumb
    RectF thumb = scrollbarThumbRect(el, scale);
    if (thumb.h > 0.f) {
        canvas.fillRoundRect(thumb, square, Color::rgb(0xc1, 0xc1, 0xc1));
    }
}

} // namespace ui
