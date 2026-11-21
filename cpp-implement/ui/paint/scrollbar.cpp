#include "paint/scrollbar.h"

#include <algorithm>

namespace ui {

bool needsScrollbar(const Element& el) {
    return el.style.overflowY == Overflow::Auto &&
           el.box.maxScrollY() > 0.5f;
}

RectF scrollbarThumbRect(const Element& el, float scale) {
    if (!needsScrollbar(el))
        return {};

    RectF client = el.box.paddingRect();
    float width = kScrollbarWidth * scale;
    float trackH = client.h;
    float contentH = std::max(el.box.contentHeight, client.h);
    float thumbH = std::max(24.f * scale, trackH * client.h / contentH);
    thumbH = std::min(trackH, thumbH);

    float maxScroll = el.box.maxScrollY();
    float progress = maxScroll > 0
        ? std::max(0.f, std::min(1.f, el.box.scrollY / maxScroll))
        : 0.f;

    float y = client.y + (trackH - thumbH) * progress;

    return {
        client.right() - width,
        y,
        width,
        thumbH
    };
}

void drawScrollbar(Canvas& canvas, const Element& el, float scale) {
    if (!needsScrollbar(el))
        return;

    RectF client = el.box.paddingRect();
    float width = kScrollbarWidth * scale;

    CornerRadii trackRadius;
    trackRadius.tl = trackRadius.tr = trackRadius.br = trackRadius.bl = 6.f * scale;

    canvas.fillRoundRect(
        { client.right() - width, client.y, width, client.h },
        trackRadius,
        Color::rgb(0x0b, 0x11, 0x20)
    );

    RectF thumb = scrollbarThumbRect(el, scale);
    CornerRadii thumbRadius;
    thumbRadius.tl = thumbRadius.tr = thumbRadius.br = thumbRadius.bl = 6.f * scale;

    canvas.fillRoundRect(thumb, thumbRadius, Color::rgb(0x33, 0x41, 0x55));
}

} // namespace ui
